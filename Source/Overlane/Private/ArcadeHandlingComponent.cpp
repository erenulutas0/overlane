#include "ArcadeHandlingComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneRaceGameState.h"

UArcadeHandlingComponent::UArcadeHandlingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UArcadeHandlingComponent::SetThrottleInput(float Value)
{
    ThrottleInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void UArcadeHandlingComponent::SetBrakeInput(float Value)
{
    BrakeInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void UArcadeHandlingComponent::SetSteeringInput(float Value)
{
    SteeringInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UArcadeHandlingComponent::SetBoostInput(bool bEnabled)
{
    bBoostRequested = bEnabled;
}

void UArcadeHandlingComponent::SetPerformanceScale(float InScale)
{
    PerformanceScale = FMath::Clamp(InScale, 0.5f, 1.15f);
}

void UArcadeHandlingComponent::ResetState(bool bRefillBoost)
{
    ThrottleInput = 0.0f;
    BrakeInput = 0.0f;
    SteeringInput = 0.0f;
    bBoostRequested = false;
    bBoostActive = false;
    CurrentSpeed = 0.0f;
    StepAccumulator = 0.0f;
    CollisionCutCooldown = 0.0f;

    if (bRefillBoost)
    {
        BoostCharge = 1.0f;
    }
}

float UArcadeHandlingComponent::GetSpeedKph() const
{
    return CurrentSpeed * 0.036f;
}

float UArcadeHandlingComponent::GetSpeedRatio() const
{
    return FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxBoostSpeed, 0.0f, 1.0f);
}

bool UArcadeHandlingComponent::IsDrivingAllowedHere() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return true;
    }

    if (const AOverlaneGameModeBase* GameMode = World->GetAuthGameMode<AOverlaneGameModeBase>())
    {
        return GameMode->IsDrivingAllowed();
    }

    // GetAuthGameMode is null on a client, so the gate above was silently a no-op
    // there and only the authority check below it hid the fact. Once the client
    // simulates its own vehicle that becomes a real bug, so fall back to the
    // replicated race state, which every machine can see.
    if (const AOverlaneRaceGameState* RaceState = World->GetGameState<AOverlaneRaceGameState>())
    {
        return RaceState->IsRaceActive() && !RaceState->IsRacePaused();
    }

    // A machine that can see neither the game mode nor the game state does not
    // know whether the race is running, and must not simulate on a guess. This
    // used to return true, which was harmless only while clients never
    // simulated - and stops being harmless the moment prediction is enabled.
    return false;
}

bool UArcadeHandlingComponent::ShouldSimulateHere() const
{
    const APawn* VehiclePawn = Cast<APawn>(GetOwner());
    return VehiclePawn && (VehiclePawn->HasAuthority() || VehiclePawn->GetNetMode() == NM_Standalone);
}

FOverlaneVehicleSimState UArcadeHandlingComponent::GetSimState() const
{
    FOverlaneVehicleSimState State;
    if (const AActor* Owner = GetOwner())
    {
        State.Location = Owner->GetActorLocation();
        State.Yaw = Owner->GetActorRotation().Yaw;
    }
    State.CurrentSpeed = CurrentSpeed;
    State.BoostCharge = BoostCharge;
    State.CollisionCutCooldown = CollisionCutCooldown;
    State.CollisionEventCount = CollisionEventCount;
    State.bBoostActive = bBoostActive;
    return State;
}

void UArcadeHandlingComponent::SetSimState(const FOverlaneVehicleSimState& InState)
{
    CurrentSpeed = InState.CurrentSpeed;
    BoostCharge = InState.BoostCharge;
    CollisionCutCooldown = InState.CollisionCutCooldown;
    CollisionEventCount = InState.CollisionEventCount;
    bBoostActive = InState.bBoostActive;

    if (AActor* Owner = GetOwner())
    {
        FRotator Rotation = Owner->GetActorRotation();
        Rotation.Yaw = InState.Yaw;
        Owner->SetActorLocationAndRotation(InState.Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void UArcadeHandlingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsDrivingAllowedHere())
    {
        // Dropping paused time is correct and symmetric: both machines do it.
        StepAccumulator = 0.0f;

        // Clearing the queue is NOT symmetric - it is the server discarding a
        // client's input, so it must only happen where the queue lives. Doing it
        // on the predicting client would silently delete its own unsimulated
        // commands and desynchronise the two sequence counters.
        if (ShouldSimulateHere())
        {
            bBoostActive = false;
            PendingCommands.Reset();
            StarveDebt = 0;
        }
        return;
    }

    if (!ShouldSimulateHere())
    {
        return;
    }

    // Clamp so a hitch, a breakpoint or a level load cannot produce a huge backlog.
    StepAccumulator += FMath::Min(DeltaTime, 0.25f);

    int32 Steps = 0;
    while (StepAccumulator >= OverlaneFixedDeltaSeconds && Steps < OverlaneMaxStepsPerFrame)
    {
        FOverlaneInputCommand Command;
        if (!ConsumeNextCommand(Command))
        {
            break;
        }

        SimulateStep(Command, OverlaneFixedDeltaSeconds, EOverlaneStepMode::Live);

        StepAccumulator -= OverlaneFixedDeltaSeconds;
        ++Steps;
    }

    // The accumulator remainder is deliberately KEPT.
    //
    // Zeroing it here, plus a catch-up loop that ran two extra steps PER FRAME
    // without debiting the accumulator, meant the server simulated more than
    // 60 steps per wall second whenever a client's input stuttered - 180/s at a
    // 60 fps host. Since the winner is decided purely from world X, the jitterier
    // connection was literally being handed free distance. The queue depth is now
    // absorbed by the starvation debt in ConsumeNextCommand instead, which keeps
    // simulated steps and wall-clock steps exactly equal.
}

void UArcadeHandlingComponent::EnqueueCommand(const FOverlaneInputCommand& Command)
{
    bCommandDriven = true;

    PendingCommands.Add(Command);

    // Bound the queue. Dropping only the single oldest command left the queue
    // pinned at the cap and silently skipped one command per push, which is a
    // discontinuity in the consumed sequence that the client would never learn
    // about. Trim back to the jitter target and raise a force-snap instead: an
    // announced discontinuity is recoverable, a silent one is not.
    if (PendingCommands.Num() > OverlaneMoveRingSize)
    {
        const int32 Excess = PendingCommands.Num() - ServerBufferTarget;
        PendingCommands.RemoveAt(0, Excess, EAllowShrinking::No);
        StarveDebt = 0;
        bPendingForceSnap = true;
    }
}

void UArcadeHandlingComponent::ClearPendingCommands()
{
    PendingCommands.Reset();
    StarveDebt = 0;
    bPendingForceSnap = true;
}

bool UArcadeHandlingComponent::ConsumePendingForceSnap()
{
    const bool bWasPending = bPendingForceSnap;
    bPendingForceSnap = false;
    return bWasPending;
}

bool UArcadeHandlingComponent::ConsumeNextCommand(FOverlaneInputCommand& OutCommand)
{
    if (!bCommandDriven)
    {
        // Locally driven: standalone, or the listen-server host's own pawn.
        OutCommand = FOverlaneInputCommand::Make(
            LocalCommandSequence++, ThrottleInput, BrakeInput, SteeringInput, bBoostRequested);
        return true;
    }

    // Pay off starvation debt before simulating anything new.
    //
    // When the queue ran dry we repeated the last command AND simulated a full
    // step for it. Simulating the real command later as well would spend two
    // steps on one command, which is the ratchet that let a stuttering client
    // accumulate free distance. Each repeated step is now recorded as a debt and
    // the matching command is retired without being simulated again.
    while (StarveDebt > 0 && PendingCommands.Num() > 0)
    {
        LastConsumedCommand = PendingCommands[0];
        LastConsumedSequence = LastConsumedCommand.Sequence;
        PendingCommands.RemoveAt(0, 1, EAllowShrinking::No);
        --StarveDebt;
    }

    if (PendingCommands.Num() > 0)
    {
        OutCommand = PendingCommands[0];
        PendingCommands.RemoveAt(0, 1, EAllowShrinking::No);
        LastConsumedCommand = OutCommand;
        LastConsumedSequence = OutCommand.Sequence;
        bInputStarved = false;
        return true;
    }

    // The client has gone quiet. Repeat its last known intent rather than
    // inventing one, and remember that we did.
    if (StarveDebt < MaxStarveDebtSteps)
    {
        OutCommand = LastConsumedCommand;
        bInputStarved = true;
        ++StarveDebt;
        return true;
    }

    // Silent for long enough that guessing is worse than holding still. Refusing
    // to simulate keeps the acked sequence and the acked state in step.
    bInputStarved = true;
    return false;
}

void UArcadeHandlingComponent::SimulateStep(const FOverlaneInputCommand& Command, float FixedDt, EOverlaneStepMode Mode)
{
    APawn* VehiclePawn = Cast<APawn>(GetOwner());
    if (!VehiclePawn)
    {
        return;
    }

    const float StepThrottle = Command.GetThrottle();
    const float StepBrake = Command.GetBrake();
    const float StepSteering = Command.GetSteering();

    // 0.01 rather than KINDA_SMALL_NUMBER: BoostCharge is quantised to 8 bits on
    // the wire, so an LSB of 1/255 straddling this threshold would let the client
    // and the server disagree about a discrete branch that swings the speed cap
    // by 1800 cm/s.
    bBoostActive = Command.IsBoostRequested()
        && StepThrottle > KINDA_SMALL_NUMBER
        && CurrentSpeed >= BoostMinimumSpeed
        && BoostCharge > 0.01f;

    if (bBoostActive)
    {
        BoostCharge = FMath::Max(0.0f, BoostCharge - (BoostDrainPerSecond * FixedDt));
    }
    else
    {
        BoostCharge = FMath::Min(1.0f, BoostCharge + (BoostRechargePerSecond * FixedDt));
    }

    // PerformanceScale is 1.0 for the human player and only moves for the AI
    // rival, where it carries both the difficulty setting and the rubber band.
    const float ActiveMaxSpeed = (bBoostActive ? MaxBoostSpeed : MaxForwardSpeed) * PerformanceScale;
    const float ActiveAcceleration = (Acceleration + (bBoostActive ? BoostAcceleration : 0.0f)) * PerformanceScale;

    if (StepThrottle > 0.0f)
    {
        if (CurrentSpeed < ActiveMaxSpeed)
        {
            CurrentSpeed = FMath::Min(CurrentSpeed + (StepThrottle * ActiveAcceleration * FixedDt), ActiveMaxSpeed);
        }
        else if (!bBoostActive)
        {
            // Releasing turbo should feel like coasting back to road speed,
            // never like an abrupt 240-to-180 km/h snap.
            CurrentSpeed = FMath::FInterpTo(CurrentSpeed, MaxForwardSpeed, FixedDt, 1.35f);
        }
    }
    else if (StepBrake > 0.0f)
    {
        const float Deceleration = StepBrake * BrakingDeceleration * FixedDt;
        CurrentSpeed = CurrentSpeed > 0.0f
            ? FMath::Max(0.0f, CurrentSpeed - Deceleration)
            : FMath::Max(-MaxReverseSpeed, CurrentSpeed - Deceleration);
    }
    else
    {
        CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, FixedDt, CoastDeceleration);
    }

    const float SpeedRatio = GetSpeedRatio();
    const float SteeringScale = FMath::Lerp(1.0f, HighSpeedSteeringMultiplier, SpeedRatio);
    const float DirectionSign = CurrentSpeed >= 0.0f ? 1.0f : -1.0f;
    const float YawDelta = StepSteering * SteeringRateDegreesPerSecond * SteeringScale * DirectionSign * FixedDt;
    VehiclePawn->AddActorWorldRotation(FRotator(0.0f, YawDelta, 0.0f));

    FHitResult Hit;
    VehiclePawn->AddActorWorldOffset(VehiclePawn->GetActorForwardVector() * CurrentSpeed * FixedDt, true, &Hit);

    CollisionCutCooldown = FMath::Max(0.0f, CollisionCutCooldown - FixedDt);

    // The sweep still blocks during a replay -- being stopped by a car that is
    // there right now is correct. What replay must NOT do is re-apply the speed
    // cut: it is a discrete branch, and flipping it on a hairline geometric
    // difference would swing the speed by 55% and make replay unstable.
    if (Mode == EOverlaneStepMode::Live
        && Hit.bBlockingHit
        && FMath::Abs(Hit.ImpactNormal.Z) < 0.7f
        && CollisionCutCooldown <= 0.0f)
    {
        CurrentSpeed *= CollisionSpeedMultiplier;
        CollisionCutCooldown = CollisionCutCooldownDuration;
        ++CollisionEventCount;
    }
}

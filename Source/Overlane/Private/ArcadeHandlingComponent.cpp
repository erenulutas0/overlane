#include "ArcadeHandlingComponent.h"

#include "GameFramework/Pawn.h"
#include "OverlaneGameModeBase.h"

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

void UArcadeHandlingComponent::ResetState()
{
    ThrottleInput = 0.0f;
    BrakeInput = 0.0f;
    SteeringInput = 0.0f;
    bBoostRequested = false;
    bBoostActive = false;
    BoostCharge = 1.0f;
    CurrentSpeed = 0.0f;
    StepAccumulator = 0.0f;
    CollisionCutCooldown = 0.0f;
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
    const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    return !GameMode || GameMode->IsDrivingAllowed();
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
        bBoostActive = false;
        // Do not bank paused time, or the race resumes with a burst of catch-up steps.
        StepAccumulator = 0.0f;
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
        // Until the network layer owns the command stream, every step samples the
        // currently cached input. Behaviour is identical to reading the scalars
        // directly, but it is already expressed as the command the wire will carry.
        const FOverlaneInputCommand Command = FOverlaneInputCommand::Make(
            LocalCommandSequence++, ThrottleInput, BrakeInput, SteeringInput, bBoostRequested);

        SimulateStep(Command, OverlaneFixedDeltaSeconds, EOverlaneStepMode::Live);

        StepAccumulator -= OverlaneFixedDeltaSeconds;
        ++Steps;
    }

    if (Steps >= OverlaneMaxStepsPerFrame)
    {
        // Drop the backlog rather than spiral: catching up is worse than a hitch.
        StepAccumulator = 0.0f;
    }
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

    bBoostActive = Command.IsBoostRequested()
        && StepThrottle > KINDA_SMALL_NUMBER
        && CurrentSpeed >= BoostMinimumSpeed
        && BoostCharge > KINDA_SMALL_NUMBER;

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

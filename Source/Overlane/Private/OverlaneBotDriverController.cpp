#include "OverlaneBotDriverController.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneVehiclePawn.h"
#include "TrafficLanePath.h"
#include "TrafficVehicleBase.h"

namespace
{
    /** Rebuilding the traffic list every frame is wasteful: pool identity is fixed. */
    constexpr float TrafficCacheInterval = 0.5f;

    /** Throttle ramps from this floor to full over ThrottleErrorSpan of speed error. */
    constexpr float ThrottleFloor = 0.35f;
    constexpr float ThrottleErrorSpan = 1400.0f;   // cm/s, ~0.5 s of Acceleration
    constexpr float BrakeDeadband = 250.0f;        // cm/s
    constexpr float BrakeErrorSpan = 2520.0f;      // cm/s, ~0.35 s of BrakingDeceleration
}

AOverlaneBotDriverController::AOverlaneBotDriverController()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
}

void AOverlaneBotDriverController::ConfigurePracticeRoute(ATrafficLanePath* InLanePath, float InStartingDistance)
{
    LanePath = InLanePath;
    StartingDistance = FMath::Max(0.0f, InStartingDistance);
    TrackedLaneDistance = -1.0f;

    TArray<ATrafficLanePath*> SortedLanes;
    ATrafficLanePath::CollectSortedLanes(this, SortedLanes);

    Lanes.Reset();
    for (ATrafficLanePath* Lane : SortedLanes)
    {
        Lanes.Add(Lane);
    }

    CurrentLaneIndex = Lanes.IndexOfByKey(InLanePath);
    TargetLaneIndex = CurrentLaneIndex;
}

void AOverlaneBotDriverController::ConfigureDifficulty(int32 Difficulty)
{
    // Seeded per race so two runs at the same difficulty are not identical.
    RaceStream.Initialize(FMath::Rand());

    switch (FMath::Clamp(Difficulty, 0, 2))
    {
    case 0:
        DifficultySpeedScale = 0.88f;
        bAllowBoost = false;
        RubberBandStrength = 0.10f;
        break;
    case 2:
        DifficultySpeedScale = 1.0f;
        bAllowBoost = true;
        RubberBandStrength = 0.03f;
        break;
    default:
        DifficultySpeedScale = 1.0f;
        bAllowBoost = true;
        RubberBandStrength = 0.06f;
        break;
    }

    DifficultySpeedScale += RaceStream.FRandRange(-0.015f, 0.015f);
    BoostEngageCharge = FMath::Clamp(BoostEngageCharge + RaceStream.FRandRange(-0.10f, 0.10f), 0.15f, 0.95f);
}

float AOverlaneBotDriverController::GetBotSpeedKph() const
{
    const AOverlaneVehiclePawn* Vehicle = Cast<AOverlaneVehiclePawn>(GetPawn());
    return Vehicle ? Vehicle->GetSpeedKph() : 0.0f;
}

void AOverlaneBotDriverController::RefreshTrafficCache(float DeltaSeconds)
{
    TrafficCacheRemaining -= DeltaSeconds;
    if (TrafficCacheRemaining > 0.0f && CachedTraffic.Num() > 0)
    {
        return;
    }

    TrafficCacheRemaining = TrafficCacheInterval;

    TArray<AActor*> FoundTraffic;
    UGameplayStatics::GetAllActorsOfClass(this, ATrafficVehicleBase::StaticClass(), FoundTraffic);

    CachedTraffic.Reset();
    for (AActor* TrafficActor : FoundTraffic)
    {
        if (ATrafficVehicleBase* TrafficVehicle = Cast<ATrafficVehicleBase>(TrafficActor))
        {
            CachedTraffic.Add(TrafficVehicle);
        }
    }
}

const ATrafficVehicleBase* AOverlaneBotDriverController::FindNearestBlocker(
    const ATrafficLanePath* Lane, float FromDistance, float& OutGap, float& OutLeaderSpeed) const
{
    OutGap = BigDistance;
    OutLeaderSpeed = 0.0f;

    if (!Lane)
    {
        return nullptr;
    }

    const ATrafficVehicleBase* Best = nullptr;
    for (const ATrafficVehicleBase* Other : CachedTraffic)
    {
        // IsOccupyingLane reports true for both lanes while a car is mid-blend,
        // which is exactly the conservative answer we want here.
        if (!Other || !Other->IsTrafficActive() || !Other->IsOccupyingLane(Lane))
        {
            continue;
        }

        const float Gap = Other->GetLaneDistance() - FromDistance - BumperAllowance;
        if (Gap > 0.0f && Gap < OutGap)
        {
            OutGap = Gap;
            OutLeaderSpeed = Other->GetCurrentSpeed();
            Best = Other;
        }
    }

    return Best;
}

void AOverlaneBotDriverController::UpdateTrackedLaneDistance(const AOverlaneVehiclePawn& Vehicle, float DeltaSeconds)
{
    const ATrafficLanePath* ActiveLane = Lanes.IsValidIndex(CurrentLaneIndex) ? Lanes[CurrentLaneIndex].Get() : LanePath.Get();
    if (!ActiveLane)
    {
        return;
    }

    const float LaneLength = ActiveLane->GetLaneLength();
    const float RawDistance = ActiveLane->GetClosestDistanceToLocation(Vehicle.GetActorLocation());

    if (TrackedLaneDistance < 0.0f)
    {
        TrackedLaneDistance = RawDistance;
        return;
    }

    // The old code clamped the lower bound to StartingDistance, so anything that
    // knocked the bot backwards froze its perceived progress permanently. Track
    // it continuously instead and only reject implausible per-frame jumps, which
    // FindInputKeyClosestToWorldLocation can produce mid lane change.
    const float MaxStep = FMath::Max(400.0f, FMath::Abs(Vehicle.GetForwardSpeedCms()) * DeltaSeconds * 2.0f);
    TrackedLaneDistance = FMath::Clamp(
        FMath::Clamp(RawDistance, TrackedLaneDistance - MaxStep, TrackedLaneDistance + MaxStep),
        0.0f,
        LaneLength);
}

bool AOverlaneBotDriverController::IsLaneChangeSafeForBot(
    const ATrafficLanePath* CandidateLane, const AOverlaneVehiclePawn& Vehicle, float BotSpeed) const
{
    if (!CandidateLane)
    {
        return false;
    }

    // Clearance scales with RELATIVE speed, not absolute speed.
    //
    // The previous version demanded max(3200, BotSpeed * 1.6) ahead, which is
    // 80 m at cruise. Traffic flows in platoons spaced at the director's own
    // following distance of ~32 m, so that window was structurally never
    // available and the bot could effectively never merge -- which is what the
    // "changes lane very slowly" report actually was. What matters is only
    // whether it would rear-end the car ahead, or be rear-ended by the car
    // behind, and both depend on the speed difference alone.
    for (const ATrafficVehicleBase* Other : CachedTraffic)
    {
        if (!Other || !Other->IsTrafficActive() || !Other->IsOccupyingLane(CandidateLane))
        {
            continue;
        }

        const float RelativeDistance = Other->GetLaneDistance() - TrackedLaneDistance;
        const float OtherSpeed = Other->GetCurrentSpeed();

        if (RelativeDistance >= 0.0f)
        {
            // The buffer here is the room needed AT THE MOMENT OF MERGING, not
            // the steady-state following distance. Demanding the full 12 m
            // settled gap made the window 17.2 m, while the director packs lanes
            // at 35 m with an 11.2 m per-lane stagger - which puts the adjacent
            // car exactly where it blocks the merge, on both sides at once. The
            // bot could therefore essentially never change lane. It is fine to
            // merge into a smaller gap and settle back afterwards; that is what
            // the follow law is for.
            const float Closing = FMath::Max(0.0f, BotSpeed - OtherSpeed);
            const float Required = MergeBufferAhead + BumperAllowance
                + ((Closing * Closing) / (2.0f * ComfortDeceleration));
            if (RelativeDistance < Required)
            {
                return false;
            }
        }
        else
        {
            // Less room is needed behind: the car back there is the one with the
            // brakes, and traffic already runs its own following law.
            const float Closing = FMath::Max(0.0f, OtherSpeed - BotSpeed);
            const float Required = MergeBufferBehind + BumperAllowance
                + ((Closing * Closing) / (2.0f * ComfortDeceleration));
            if (-RelativeDistance < Required)
            {
                return false;
            }
        }
    }

    // Physical merge safety against the human only. Deliberately not the traffic
    // system's 60 m player exclusion: a rival's whole job is to pass the player.
    const FVector CandidateLocation = CandidateLane->GetTransformAtDistance(TrackedLaneDistance).GetLocation();
    for (TActorIterator<AOverlaneVehiclePawn> PawnIt(GetWorld()); PawnIt; ++PawnIt)
    {
        const AOverlaneVehiclePawn* OtherPawn = *PawnIt;
        if (!OtherPawn || OtherPawn == &Vehicle)
        {
            continue;
        }

        if (FVector::Dist2D(OtherPawn->GetActorLocation(), CandidateLocation) < RivalPawnClearance)
        {
            return false;
        }
    }

    return true;
}

float AOverlaneBotDriverController::ComputeFollowSpeed(float Gap, float LeaderSpeed, float CruiseSpeed) const
{
    // Safe-following speed: what we can carry and still bleed down to the
    // leader's speed before the buffer closes. Continuous and monotonic in Gap,
    // so unlike a speed-scaled threshold there is no boundary to oscillate on.
    const float ApproachGap = Gap - MinimumFollowingDistance;
    if (ApproachGap <= 0.0f)
    {
        return LeaderSpeed * FMath::Clamp(Gap / FMath::Max(MinimumFollowingDistance, 1.0f), 0.0f, 1.0f);
    }

    if (Gap >= BigDistance)
    {
        return CruiseSpeed;
    }

    return FMath::Min(CruiseSpeed, LeaderSpeed + FMath::Sqrt(2.0f * ComfortDeceleration * ApproachGap));
}

float AOverlaneBotDriverController::ComputeLaneSpeedPotential(const ATrafficLanePath* Lane, float CruiseSpeed) const
{
    float Gap = BigDistance;
    float LeaderSpeed = 0.0f;
    FindNearestBlocker(Lane, TrackedLaneDistance, Gap, LeaderSpeed);
    return ComputeFollowSpeed(Gap, LeaderSpeed, CruiseSpeed);
}

void AOverlaneBotDriverController::ConsiderOvertake(
    const AOverlaneVehiclePawn& Vehicle, float CurrentSpeedPotential, float CruiseSpeed, float BotSpeed)
{
    if (TargetLaneIndex != CurrentLaneIndex || LaneChangeCooldown > 0.0f)
    {
        return;
    }

    if (BlockedSeconds < BlockedTimeToOvertake || !Lanes.IsValidIndex(CurrentLaneIndex))
    {
        return;
    }

    const ATrafficLanePath* ActiveLane = Lanes[CurrentLaneIndex].Get();
    if (!ActiveLane || TrackedLaneDistance > ActiveLane->GetLaneLength() - LaneChangeEndBuffer)
    {
        return;
    }

    // Score lanes by the speed they would actually allow, not by raw gap.
    //
    // The required gain collapses when the current lane is barely moving. The
    // follow law's fixed point makes the bot an exact speed copy of whatever is
    // in front of it, including a standstill - so a rival that arrives behind a
    // stopped queue inherits the standstill and, with the usual gain threshold,
    // can never justify leaving it. Any lane that moves at all beats a lane that
    // does not.
    const float StalledPotential = 400.0f;
    const float RequiredGain = CurrentSpeedPotential < StalledPotential ? 60.0f : OvertakeSpeedGain;

    int32 BestIndex = INDEX_NONE;
    float BestScore = CurrentSpeedPotential + RequiredGain;

    for (const int32 Offset : { -1, 1 })
    {
        const int32 CandidateIndex = CurrentLaneIndex + Offset;
        if (!Lanes.IsValidIndex(CandidateIndex))
        {
            continue;
        }

        ATrafficLanePath* CandidateLane = Lanes[CandidateIndex].Get();
        const float CandidateSpeed = ComputeLaneSpeedPotential(CandidateLane, CruiseSpeed);

        if (CandidateSpeed <= BestScore)
        {
            ++RejectedByGain;
            continue;
        }

        if (!IsLaneChangeSafeForBot(CandidateLane, Vehicle, BotSpeed))
        {
            ++RejectedBySafety;
            continue;
        }

        BestScore = CandidateSpeed;
        BestIndex = CandidateIndex;
    }

    if (BestIndex != INDEX_NONE)
    {
        TargetLaneIndex = BestIndex;
        LaneChangeElapsed = 0.0f;
        ++MergesStarted;
    }
}

float AOverlaneBotDriverController::ComputeRubberBandScale(const AOverlaneVehiclePawn& Vehicle) const
{
    const APawn* HumanPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!HumanPawn || HumanPawn == &Vehicle)
    {
        return DifficultySpeedScale;
    }

    const float BotX = Vehicle.GetActorLocation().X;

    // Freeze the band near the finish so the last stretch is an honest race.
    const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (GameMode && GameMode->GetRouteFinishX() - BotX < RubberBandFreezeDistance)
    {
        return DifficultySpeedScale;
    }

    // World X, not lane distance: both finish tests use X, so anything else could
    // disagree with who actually wins.
    const float GapMeters = (HumanPawn->GetActorLocation().X - BotX) / 100.0f;
    const float BandAlpha = FMath::Clamp(GapMeters / RubberBandRangeMeters, -1.0f, 1.0f);
    return DifficultySpeedScale * (1.0f + (BandAlpha * RubberBandStrength));
}

void AOverlaneBotDriverController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // This controller is intentionally server-only. The possessed pawn uses the
    // existing movement replication so clients would see the same bot.
    if (!HasAuthority())
    {
        return;
    }

    AOverlaneVehiclePawn* Vehicle = Cast<AOverlaneVehiclePawn>(GetPawn());
    const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (!Vehicle || !GameMode || !GameMode->IsDrivingAllowed() || Lanes.Num() == 0)
    {
        StopVehicleInput();
        return;
    }

    if (!Lanes.IsValidIndex(CurrentLaneIndex) || !Lanes.IsValidIndex(TargetLaneIndex))
    {
        StopVehicleInput();
        return;
    }

    LaneChangeCooldown = FMath::Max(0.0f, LaneChangeCooldown - DeltaSeconds);
    RivalContactCooldown = FMath::Max(0.0f, RivalContactCooldown - DeltaSeconds);

    const ATrafficLanePath* ActiveLane = Lanes[CurrentLaneIndex].Get();
    const ATrafficLanePath* SteerLane = Lanes[TargetLaneIndex].Get();
    if (!ActiveLane || !SteerLane)
    {
        StopVehicleInput();
        return;
    }

    const float LaneLength = ActiveLane->GetLaneLength();
    if (LaneLength <= KINDA_SMALL_NUMBER)
    {
        StopVehicleInput();
        return;
    }

    RefreshTrafficCache(DeltaSeconds);
    UpdateTrackedLaneDistance(*Vehicle, DeltaSeconds);

    const float BotSpeed = Vehicle->GetForwardSpeedCms();

    // ---- Lane change progress ------------------------------------------------
    if (TargetLaneIndex != CurrentLaneIndex)
    {
        LaneChangeElapsed += DeltaSeconds;

        const FTransform TargetLaneTransform = SteerLane->GetTransformAtDistance(TrackedLaneDistance);
        const float LateralOffset = FMath::Abs(TargetLaneTransform.GetLocation().Y - Vehicle->GetActorLocation().Y);

        // Position alone is not enough: the car reaches the tolerance band still
        // carrying outward yaw, and nothing sheds it, which is where most of the
        // residual overshoot into the barrier came from. Require the heading to
        // be lined up with the lane as well.
        const float HeadingOffsetDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(
            Vehicle->GetActorRotation().Yaw, TargetLaneTransform.Rotator().Yaw));

        if (LateralOffset < LaneCompleteTolerance && HeadingOffsetDegrees < LaneCompleteHeadingToleranceDegrees)
        {
            CurrentLaneIndex = TargetLaneIndex;
            LanePath = Lanes[CurrentLaneIndex];

            // Re-project onto the new spline NOW rather than invalidating with
            // -1. UpdateTrackedLaneDistance has already run this tick and the
            // steering block below consumes this value, so a -1 put the aim point
            // ~1.5 km behind the car, tripped the spun-around branch and slammed
            // full lock toward the side the car was already overshooting. The
            // step clamp is kept so a spline projection glitch cannot teleport it.
            const float ReseedDistance = Lanes[CurrentLaneIndex]->GetClosestDistanceToLocation(Vehicle->GetActorLocation());
            TrackedLaneDistance = FMath::Clamp(ReseedDistance, TrackedLaneDistance - 400.0f, TrackedLaneDistance + 400.0f);

            ++MergesCompleted;
            LaneChangeCooldown = 2.0f;
        }
        else if (LaneChangeElapsed > LaneChangeTimeout)
        {
            TargetLaneIndex = CurrentLaneIndex;
            LaneChangeCooldown = 1.5f;
        }
    }

    // ---- Perception ----------------------------------------------------------
    float Gap = BigDistance;
    float LeaderSpeed = 0.0f;
    const ATrafficVehicleBase* Blocker = FindNearestBlocker(ActiveLane, TrackedLaneDistance, Gap, LeaderSpeed);

    // While merging, the target lane blocks us too.
    if (TargetLaneIndex != CurrentLaneIndex)
    {
        float TargetGap = BigDistance;
        float TargetLeaderSpeed = 0.0f;
        if (FindNearestBlocker(SteerLane, TrackedLaneDistance, TargetGap, TargetLeaderSpeed) && TargetGap < Gap)
        {
            Gap = TargetGap;
            LeaderSpeed = TargetLeaderSpeed;
            Blocker = nullptr;
        }
    }

    LastGapToBlocker = Gap;

    const float RubberBandedScale = ComputeRubberBandScale(*Vehicle);
    Vehicle->SetPerformanceScale(RubberBandedScale);

    // MaxForwardSpeed is 5000 cm/s; the pawn applies the scale internally, so the
    // cruise target here uses the same scaled number the handling will allow.
    // While boosting the pawn's ceiling rises to MaxBoostSpeed, and the target
    // has to rise with it or the throttle controller backs off and cancels the
    // boost it just asked for.
    const float BoostCeiling = (bAllowBoost && bBoostEngaged) ? 6800.0f : 5000.0f;
    const float CruiseSpeed = BoostCeiling * RubberBandedScale;

    const float DesiredSpeed = ComputeFollowSpeed(Gap, LeaderSpeed, CruiseSpeed);

    bBlockedAhead = DesiredSpeed < CruiseSpeed * 0.9f;
    BlockedSeconds = bBlockedAhead ? BlockedSeconds + DeltaSeconds : 0.0f;

    ConsiderOvertake(*Vehicle, DesiredSpeed, CruiseSpeed, BotSpeed);

    // ---- Steering ------------------------------------------------------------
    // Yaw authority falls from 105 deg/s toward 33.6 deg/s as speed rises, so a
    // fixed look-ahead puts the target inside the achievable turn radius and the
    // controller weaves. Scale it with speed and damp the error rate.
    // A shorter aim point while merging was tried and reverted: halving the
    // look-ahead halves the pursuit time constant and drops the damping ratio
    // from 0.42 to 0.19, which more than doubled the overshoot into the barrier.
    // The look-ahead stays speed-scaled and continuous through the whole merge.
    const float LookAhead = FMath::Clamp(LookAheadTimeSeconds * FMath::Abs(BotSpeed), MinLookAheadDistance, MaxLookAheadDistance);
    const float TargetDistance = FMath::Min(TrackedLaneDistance + LookAhead, SteerLane->GetLaneLength());
    const FVector TargetLocation = SteerLane->GetTransformAtDistance(TargetDistance).GetLocation();
    const FVector LocalTarget = Vehicle->GetActorTransform().InverseTransformPositionNoScale(TargetLocation);

    float Throttle = 0.0f;
    float Brake = 0.0f;
    const bool bSpunAround = LocalTarget.X < 0.0f;

    // Every path below writes RawSteering and then goes through the SAME slew
    // limiter. The spin-recovery branch used to assign SmoothedSteering directly,
    // which put a 2.0-wide step into yaw RATE in one frame and took ~290 ms to
    // unwind -- a standing hazard any time a transient made the aim point land
    // behind the car.
    float RawSteering = 0.0f;

    if (bSpunAround)
    {
        // Facing backwards. Steering authority is best at rest, so stop first.
        RawSteering = LocalTarget.Y >= 0.0f ? 1.0f : -1.0f;
        if (BotSpeed > 200.0f)
        {
            Brake = 1.0f;
        }
        else
        {
            Throttle = 0.35f;
        }
        PreviousHeadingError = 0.0f;
    }
    else
    {
        const float HeadingError = FMath::Atan2(LocalTarget.Y, FMath::Max(LocalTarget.X, 1.0f));

        // The aim point steps a full lane sideways when a merge starts, so the
        // error derivative spikes. Suppressing it that frame is cheap insurance,
        // though with SteeringDampingGain at 0 it is currently inert.
        const bool bSteeringTargetJumped = SteeringTargetLaneIndex != TargetLaneIndex;
        SteeringTargetLaneIndex = TargetLaneIndex;
        if (bSteeringTargetJumped)
        {
            PreviousHeadingError = HeadingError;
        }

        const float ErrorRate = (HeadingError - PreviousHeadingError) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
        PreviousHeadingError = HeadingError;

        RawSteering = (SteeringProportionalGain * HeadingError) - (SteeringDampingGain * ErrorRate);
    }

    // The pawn integrates the command into yaw, so a step in the command is a
    // step in yaw RATE. Slew-limit the command, do not merely clamp it.
    {
        const float MaxDelta = SteeringSlewRate * DeltaSeconds;
        SmoothedSteering = FMath::Clamp(
            FMath::Clamp(RawSteering, -1.0f, 1.0f),
            SmoothedSteering - MaxDelta,
            SmoothedSteering + MaxDelta);
    }

    if (!bSpunAround)
    {
        // ---- Throttle and brake ---------------------------------------------
        // These are mutually exclusive in the handling component: any non-zero
        // throttle makes the brake branch unreachable.
        const float SpeedError = DesiredSpeed - BotSpeed;
        if (Gap < EmergencyGap)
        {
            Brake = 1.0f;
        }
        else if (SpeedError < -BrakeDeadband)
        {
            Brake = FMath::Clamp(-SpeedError / BrakeErrorSpan, 0.20f, 1.0f);
        }
        else if (SpeedError > 0.0f)
        {
            Throttle = FMath::Clamp(ThrottleFloor + (SpeedError / ThrottleErrorSpan), 0.0f, 1.0f);
        }
        // else: coast, which is the correct gentle lift-off.
    }

    // ---- Contact with the human ----------------------------------------------
    const bool bRivalContact = Vehicle->IsRivalContactFeedbackActive();
    if (bRivalContact && !bWasRivalContactActive)
    {
        RivalContactCooldown = 1.0f;
    }
    bWasRivalContactActive = bRivalContact;

    if (RivalContactCooldown > 0.65f)
    {
        // Contact costs the bot something, so it visibly backs off rather than
        // leaning on the player.
        Throttle = 0.0f;
        Brake = FMath::Max(Brake, 0.4f);
    }

    // ---- Boost ---------------------------------------------------------------
    if (!bBoostEngaged && Vehicle->GetBoostChargeRatio() > BoostEngageCharge)
    {
        bBoostEngaged = true;
    }
    else if (bBoostEngaged && Vehicle->GetBoostChargeRatio() < BoostReleaseCharge)
    {
        bBoostEngaged = false;
    }

    // Deliberately NOT gated on throttle magnitude.
    //
    // Throttle falls as the bot approaches its target, so a `Throttle > 0.5`
    // gate cut boost out BEFORE the bot reached its own cruise speed - meaning
    // the rival could never get anywhere near the 244.8 km/h the human uses, and
    // no deficit was ever recoverable. Clear road and enough speed to be worth
    // boosting are the real conditions.
    const bool bWantsBoost = bAllowBoost
        && bBoostEngaged
        && !bBlockedAhead
        && !bSpunAround
        && Throttle > KINDA_SMALL_NUMBER
        && BotSpeed >= 900.0f
        && FMath::Abs(SmoothedSteering) < 0.35f
        && TargetLaneIndex == CurrentLaneIndex
        && RivalContactCooldown <= 0.0f;

    Vehicle->SetThrottleInput(Throttle);
    Vehicle->SetBrakeInput(Brake);
    Vehicle->SetSteeringInput(SmoothedSteering);
    Vehicle->SetBoostInput(bWantsBoost);
}

void AOverlaneBotDriverController::StopVehicleInput() const
{
    if (AOverlaneVehiclePawn* Vehicle = Cast<AOverlaneVehiclePawn>(GetPawn()))
    {
        Vehicle->SetThrottleInput(0.0f);
        Vehicle->SetBrakeInput(0.0f);
        Vehicle->SetSteeringInput(0.0f);
        Vehicle->SetBoostInput(false);
    }
}

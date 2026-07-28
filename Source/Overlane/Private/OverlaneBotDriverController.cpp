#include "OverlaneBotDriverController.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneVehiclePawn.h"
#include "TrafficDirector.h"
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

    // Asymmetric: an overtaking car needs far more room ahead than behind, and
    // both windows scale with how fast it is actually travelling.
    const float AheadClearance = FMath::Max(3200.0f, BotSpeed * 1.6f);
    const float BehindClearance = 900.0f;

    for (const ATrafficVehicleBase* Other : CachedTraffic)
    {
        if (!Other || !Other->IsTrafficActive() || !Other->IsOccupyingLane(CandidateLane))
        {
            continue;
        }

        const float RelativeDistance = Other->GetLaneDistance() - TrackedLaneDistance;
        if (RelativeDistance > -BehindClearance && RelativeDistance < AheadClearance)
        {
            return false;
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

void AOverlaneBotDriverController::ConsiderOvertake(const AOverlaneVehiclePawn& Vehicle, float CurrentGap, float BotSpeed)
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

    int32 BestIndex = INDEX_NONE;
    float BestScore = CurrentGap + OvertakeGainMargin;

    for (const int32 Offset : { -1, 1 })
    {
        const int32 CandidateIndex = CurrentLaneIndex + Offset;
        if (!Lanes.IsValidIndex(CandidateIndex))
        {
            continue;
        }

        ATrafficLanePath* CandidateLane = Lanes[CandidateIndex].Get();
        float CandidateGap = BigDistance;
        float CandidateLeaderSpeed = 0.0f;
        FindNearestBlocker(CandidateLane, TrackedLaneDistance, CandidateGap, CandidateLeaderSpeed);

        if (CandidateGap > BestScore && IsLaneChangeSafeForBot(CandidateLane, Vehicle, BotSpeed))
        {
            BestScore = CandidateGap;
            BestIndex = CandidateIndex;
        }
    }

    if (BestIndex != INDEX_NONE)
    {
        TargetLaneIndex = BestIndex;
        LaneChangeElapsed = 0.0f;
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

        const FVector TargetLaneLocation = SteerLane->GetTransformAtDistance(TrackedLaneDistance).GetLocation();
        const float LateralOffset = FMath::Abs(TargetLaneLocation.Y - Vehicle->GetActorLocation().Y);

        if (LateralOffset < LaneCompleteTolerance)
        {
            CurrentLaneIndex = TargetLaneIndex;
            LanePath = Lanes[CurrentLaneIndex];
            TrackedLaneDistance = -1.0f;   // re-seed against the new spline
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
    const float CruiseSpeed = 5000.0f * RubberBandedScale;

    const float ClosingSpeed = FMath::Max(BotSpeed - LeaderSpeed, 0.0f);
    const float DistanceScale = FMath::Max(1.0f, ClosingSpeed / ReferenceClosingSpeed);
    const float MinGap = MinimumFollowingDistance * DistanceScale;
    const float MaxGap = FollowingDistance * DistanceScale;

    float DesiredSpeed = CruiseSpeed;
    if (Gap < MaxGap)
    {
        DesiredSpeed = FMath::Min(DesiredSpeed, ATrafficDirector::ComputeFollowSpeedLimit(LeaderSpeed, Gap, MinGap, MaxGap));
    }

    bBlockedAhead = DesiredSpeed < CruiseSpeed * 0.9f;
    BlockedSeconds = bBlockedAhead ? BlockedSeconds + DeltaSeconds : 0.0f;

    ConsiderOvertake(*Vehicle, Gap, BotSpeed);

    // ---- Steering ------------------------------------------------------------
    // Yaw authority falls from 105 deg/s toward 33.6 deg/s as speed rises, so a
    // fixed look-ahead puts the target inside the achievable turn radius and the
    // controller weaves. Scale it with speed and damp the error rate.
    const float LookAhead = FMath::Clamp(LookAheadTimeSeconds * FMath::Abs(BotSpeed), MinLookAheadDistance, MaxLookAheadDistance);
    const float TargetDistance = FMath::Min(TrackedLaneDistance + LookAhead, SteerLane->GetLaneLength());
    const FVector TargetLocation = SteerLane->GetTransformAtDistance(TargetDistance).GetLocation();
    const FVector LocalTarget = Vehicle->GetActorTransform().InverseTransformPositionNoScale(TargetLocation);

    float Throttle = 0.0f;
    float Brake = 0.0f;
    const bool bSpunAround = LocalTarget.X < 0.0f;

    if (bSpunAround)
    {
        // Facing backwards. Steering authority is best at rest, so stop first.
        SmoothedSteering = LocalTarget.Y >= 0.0f ? 1.0f : -1.0f;
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
        const float ErrorRate = (HeadingError - PreviousHeadingError) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
        PreviousHeadingError = HeadingError;

        const float RawSteering = (SteeringProportionalGain * HeadingError) - (SteeringDampingGain * ErrorRate);

        // The pawn integrates the command into yaw, so a step in the command is a
        // step in yaw RATE. Slew-limit the command, do not merely clamp it.
        const float MaxDelta = SteeringSlewRate * DeltaSeconds;
        SmoothedSteering = FMath::Clamp(
            FMath::Clamp(RawSteering, -1.0f, 1.0f),
            SmoothedSteering - MaxDelta,
            SmoothedSteering + MaxDelta);

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

    const bool bWantsBoost = bAllowBoost
        && bBoostEngaged
        && !bBlockedAhead
        && !bSpunAround
        && Throttle > 0.5f
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

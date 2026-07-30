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
    /**
     * Fixes the rival's per-race jitter so a measurement can be repeated.
     *
     * ConfigureDifficulty deliberately jitters DifficultySpeedScale by +/-1.5% and
     * BoostEngageCharge by +/-0.10 so two races are not identical. That is right for
     * shipping and wrong for measuring: +/-0.10 on the engage charge materially moves
     * when boost arms, so telemetry compared BETWEEN runs was partly reading the draw
     * rather than the change under test. Several cross-run comparisons on this bot
     * were reported with more confidence than that noise floor supported.
     *
     * 0 keeps the shipping behaviour. Non-zero pins every stream that feeds the rival.
     */
    static TAutoConsoleVariable<int32> CVarBotSeed(
        TEXT("overlane.Bot.Seed"),
        0,
        TEXT("Rival bot RNG seed. 0 = random per race. Non-zero = fixed, for reproducible measurement."),
        ECVF_Default);

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
    // Seeded per race so two runs at the same difficulty are not identical, unless
    // overlane.Bot.Seed pins it. The seed is logged either way, so a run that produced
    // interesting telemetry can be replayed exactly.
    const int32 ConfiguredSeed = CVarBotSeed.GetValueOnAnyThread();
    const int32 UsedSeed = ConfiguredSeed != 0 ? ConfiguredSeed : FMath::Rand();
    RaceStream.Initialize(UsedSeed);
    AppliedDifficulty = FMath::Clamp(Difficulty, 0, 2);
    UE_LOG(LogTemp, Log, TEXT("[Overlane] rival seed %d (difficulty %d)"), UsedSeed, AppliedDifficulty);

    switch (FMath::Clamp(Difficulty, 0, 2))
    {
    case 0:
        DifficultySpeedScale = 0.88f;
        bAllowBoost = false;
        RubberBandStrength = 0.10f;

        // An easy rival hesitates and leaves more room, which is what makes it
        // beatable without simply making it slow.
        BlockedTimeToOvertake = 0.7f;
        PostMergeCooldown = 1.8f;
        break;
    case 2:
        DifficultySpeedScale = 1.0f;
        bAllowBoost = true;
        RubberBandStrength = 0.03f;

        // Difficulty as SKILL, not as a speed multiplier: a hard rival commits to
        // gaps sooner, recovers between passes faster, and accepts less margin.
        BlockedTimeToOvertake = 0.22f;
        PostMergeCooldown = 0.6f;
        // Still floored above EmergencyGap: a hard rival accepts less margin, but
        // authorising a merge the follow law will answer with a full brake makes it
        // slower, not harder. It also crosses sooner before yielding the home lane.
        MergeBufferAhead = 620.0f;
        MergeBufferBehind = 110.0f;
        LateralClearFraction = 0.45f;
        break;
    default:
        DifficultySpeedScale = 1.0f;
        bAllowBoost = true;
        RubberBandStrength = 0.06f;
        break;
    }

    DifficultySpeedScale += RaceStream.FRandRange(-0.015f, 0.015f);
    // Jitter narrowed with the band. At +/-0.10 around a 0.55 engage point this was
    // moving the rival's whole boost cadence from race to race, which is most of why
    // cross-run telemetry was unusable. Around a 0.05 floor that spread would also be
    // meaningless - it would swing the floor by 200%.
    BoostEngageCharge = FMath::Clamp(BoostEngageCharge + RaceStream.FRandRange(-0.015f, 0.015f), 0.02f, 0.20f);
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

        // "Is it ahead" and "how much clear air is there" are separate questions,
        // and conflating them made the bot BLIND in the one band that matters.
        //
        // This used to subtract BumperAllowance first and then test Gap > 0, so any
        // car whose raw lead was between 0 and 520 cm produced a negative gap and was
        // skipped entirely. The bot then took the NEXT car, 3500 cm further on, as its
        // leader and applied throttle into the back of the one it was already
        // overlapping - the swept offset stopped it and the collision speed cut
        // punished it, from a state it could not see coming.
        //
        // Ahead-ness is decided on the raw lead; the reported clearance floors at
        // zero, which drives ComputeFollowSpeed into its sub-buffer branch and trips
        // the EmergencyGap full brake. That is the correct response to overlapping a
        // car, and it is now reachable.
        const float RawLead = Other->GetLaneDistance() - FromDistance;
        if (RawLead <= 0.0f)
        {
            continue;
        }

        const float Clearance = FMath::Max(0.0f, RawLead - BumperAllowance);
        if (Clearance < OutGap)
        {
            OutGap = Clearance;
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
            // Planned at MergeDeceleration, not ComfortDeceleration. The bot is
            // deciding whether it CAN take this gap, not how gently it would like
            // to trail the car once it is in it. Only lanes that already beat the
            // current one on projected ground reach this test, so "is it worth it"
            // is settled by the time we get here; the only question left is
            // physical, and physically the pawn can brake at 7200.
            const float Closing = FMath::Max(0.0f, BotSpeed - OtherSpeed);
            const float Required = MergeBufferAhead + BumperAllowance
                + ((Closing * Closing) / (2.0f * MergeDeceleration));
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

float AOverlaneBotDriverController::ProjectDistance(
    float Gap, float LeaderSpeed, float CruiseSpeed, float BotSpeed) const
{
    // A clear lane needs no special case: ComputeFollowSpeed already returns
    // CruiseSpeed for a BigDistance gap, so the roll-out below just integrates the
    // climb to cruise, which is exactly the number wanted.
    const float StepSeconds = 0.25f;
    const int32 StepCount = FMath::Max(1, FMath::CeilToInt(LaneScoreHorizon / StepSeconds));

    float Speed = FMath::Max(0.0f, BotSpeed);
    float LeaderAhead = Gap;
    float Travelled = 0.0f;

    for (int32 Step = 0; Step < StepCount; ++Step)
    {
        const float Target = ComputeFollowSpeed(LeaderAhead, LeaderSpeed, CruiseSpeed);

        // First-order tracking, matching how the throttle controller closes on a
        // target rather than snapping to it, so the score reflects reachable speed.
        Speed = FMath::FInterpTo(Speed, Target, StepSeconds, SpeedTrackingRate);
        Travelled += Speed * StepSeconds;

        // The bot cannot drive through its leader, so the gap floors at zero
        // instead of going negative and re-entering the follow law as a stop.
        LeaderAhead = FMath::Max(0.0f, LeaderAhead + ((LeaderSpeed - Speed) * StepSeconds));
    }

    return Travelled;
}

float AOverlaneBotDriverController::ComputeLaneProjectedDistance(
    const ATrafficLanePath* Lane, float CruiseSpeed, float BotSpeed) const
{
    float Gap = BigDistance;
    float LeaderSpeed = 0.0f;
    FindNearestBlocker(Lane, TrackedLaneDistance, Gap, LeaderSpeed);
    return ProjectDistance(Gap, LeaderSpeed, CruiseSpeed, BotSpeed);
}

void AOverlaneBotDriverController::ConsiderOvertake(
    const AOverlaneVehiclePawn& Vehicle, float CurrentLaneProjection, float CruiseSpeed, float BotSpeed)
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

    // Score lanes by ground covered over the horizon, not by instantaneous speed.
    //
    // The required gain collapses when the current lane is barely moving. The
    // follow law's fixed point makes the bot an exact speed copy of whatever is
    // in front of it, including a standstill - so a rival that arrives behind a
    // stopped queue inherits the standstill and, with the usual gain threshold,
    // can never justify leaving it. Any lane that moves at all beats a lane that
    // does not.
    const float StalledProjection = 400.0f * LaneScoreHorizon;
    const float RequiredGain = CurrentLaneProjection < StalledProjection
        ? 0.1f * OvertakeDistanceGain
        : OvertakeDistanceGain;

    int32 BestIndex = INDEX_NONE;
    float BestScore = CurrentLaneProjection + RequiredGain;

    for (const int32 Offset : { -1, 1 })
    {
        const int32 CandidateIndex = CurrentLaneIndex + Offset;
        if (!Lanes.IsValidIndex(CandidateIndex))
        {
            continue;
        }

        ATrafficLanePath* CandidateLane = Lanes[CandidateIndex].Get();
        const float CandidateProjection = ComputeLaneProjectedDistance(CandidateLane, CruiseSpeed, BotSpeed);

        if (CandidateProjection <= BestScore)
        {
            ++RejectedByGain;
            continue;
        }

        if (!IsLaneChangeSafeForBot(CandidateLane, Vehicle, BotSpeed))
        {
            ++RejectedBySafety;
            continue;
        }

        BestScore = CandidateProjection;
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
    // 1.0 means "not crossing, or laterally arrived"; 0.0 means "still on the home
    // lane centre". Read below to decide how much the car being passed still governs.
    float CrossingProgress = 1.0f;

    if (TargetLaneIndex != CurrentLaneIndex)
    {
        LaneChangeElapsed += DeltaSeconds;

        const FTransform TargetLaneTransform = SteerLane->GetTransformAtDistance(TrackedLaneDistance);
        const float LateralOffset = FMath::Abs(TargetLaneTransform.GetLocation().Y - Vehicle->GetActorLocation().Y);

        // Lanes are TrafficLaneSpacing apart, so the remaining offset IS the progress.
        CrossingProgress = FMath::Clamp(1.0f - (LateralOffset / 600.0f), 0.0f, 1.0f);

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
            LaneChangeCooldown = PostMergeCooldown;
        }
        else if (LaneChangeElapsed > LaneChangeTimeout)
        {
            TargetLaneIndex = CurrentLaneIndex;
            LaneChangeCooldown = AbortedMergeCooldown;
        }
    }

    // ---- Perception ----------------------------------------------------------
    //
    // The DESTINATION lane sets the speed target; the lane being left applies only a
    // floor that fades as the bot crosses. Outside a merge these are the same lane, so
    // this is the ordinary follow law.
    //
    // The old form took min(home, destination) for the whole crossing, and that single
    // min is why the rival never overtook anything. A pass is committed from the
    // follow-law fixed point, where the home leader's clearance is exactly
    // MinimumFollowingDistance and ComputeFollowSpeed therefore returns the home
    // leader's speed EXACTLY. Taking the min pinned the target to that value for the
    // entire crossing, so the bot translated sideways at the speed of the car it was
    // trying to pass and arrived alongside having gained nothing.
    float Gap = BigDistance;
    float LeaderSpeed = 0.0f;
    FindNearestBlocker(SteerLane, TrackedLaneDistance, Gap, LeaderSpeed);

    float HomeGap = BigDistance;
    float HomeLeaderSpeed = 0.0f;
    float HomeOverlap = 0.0f;
    if (TargetLaneIndex != CurrentLaneIndex)
    {
        // The bot is still physically in the home lane until it is laterally clear, so
        // the car being passed can still be hit. Weight it by how much of the bot is
        // still in that lane rather than dropping it outright.
        HomeOverlap = 1.0f - FMath::Clamp(CrossingProgress / FMath::Max(LateralClearFraction, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        if (HomeOverlap > 0.0f && !FindNearestBlocker(ActiveLane, TrackedLaneDistance, HomeGap, HomeLeaderSpeed))
        {
            HomeOverlap = 0.0f;
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

    float DesiredSpeed = ComputeFollowSpeed(Gap, LeaderSpeed, CruiseSpeed);

    // The car being passed, weighted by how much of the bot still shares its lane.
    // At HomeOverlap 1 this is the old hard min, which is correct while the bot is
    // still on the home lane centre; by LateralClearFraction it is gone and the pass
    // speed answers only to the destination lane. This is the whole overtake.
    if (HomeOverlap > 0.0f)
    {
        const float HomeLimit = ComputeFollowSpeed(HomeGap, HomeLeaderSpeed, CruiseSpeed);
        DesiredSpeed = FMath::Lerp(DesiredSpeed, FMath::Min(DesiredSpeed, HomeLimit), HomeOverlap);
    }

    // Kept instantaneous, and used only to gate boost: spending charge while
    // closing on a car that is about to force a brake is wasted charge.
    //
    // Measured against the UNBOOSTED ceiling, deliberately. Against CruiseSpeed this
    // was a self-lock that made boost dead code in traffic, which is why allowing
    // boost during a merge changed nothing at all:
    //   boost armed   -> CruiseSpeed 6800 -> needs DesiredSpeed >= 6120 -> Gap >= 4645
    //   boost disarmed -> CruiseSpeed 5000 -> needs DesiredSpeed >= 4500 -> Gap >= 2103
    // The field is packed at TrafficSpacing 3500 minus BumperAllowance, so 2980 cm is
    // the largest gap that exists. Arming boost moved the bar from reachable to
    // unreachable, bWantsBoost also requires bBoostEngaged so the reachable window
    // was unusable, and because boost never fired the charge never drained - so it
    // stayed armed forever. The rival never boosted in traffic at any difficulty.
    // A fixed reference means arming boost cannot move its own goalposts.
    const float BlockedReference = 5000.0f * RubberBandedScale;
    bBlockedAhead = DesiredSpeed < BlockedReference * 0.9f;

    // The overtake trigger, deliberately NOT the same test.
    //
    // Tied to bBlockedAhead, the rival only began looking for a way out once it
    // was already down to 0.9 * cruise, which on this traffic is about 66 m
    // behind a slow car - roughly 1.4 s of warning at closing speed, less than
    // one merge takes. It therefore always arrived at the follow law's fixed
    // point first, and from there every option looks equally bad. Comparing the
    // roll-out against clear road instead raises the alarm while the bot is
    // still fast and still has room to pick a gap.
    const float CurrentLaneProjection = ProjectDistance(Gap, LeaderSpeed, CruiseSpeed, BotSpeed);
    const float FreeProjection = ProjectDistance(BigDistance, 0.0f, CruiseSpeed, BotSpeed);

    const bool bLaneCostsGround = CurrentLaneProjection < FreeProjection - OvertakeDistanceGain;
    BlockedSeconds = bLaneCostsGround ? BlockedSeconds + DeltaSeconds : 0.0f;

    ConsiderOvertake(*Vehicle, CurrentLaneProjection, CruiseSpeed, BotSpeed);

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

        // The emergency test is physical, so it takes the nearer of the two lanes
        // whenever the bot still overlaps the one it is leaving. The speed TARGET
        // above releases the home car progressively; a collision does not care about
        // that weighting, only about whether a car is actually there.
        const float EmergencyRelevantGap = HomeOverlap > 0.0f ? FMath::Min(Gap, HomeGap) : Gap;
        if (EmergencyRelevantGap < EmergencyGap)
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
        && RivalContactCooldown <= 0.0f;

    // The same-lane requirement that used to sit in that list is gone. Overtaking
    // is exactly when a rival should be spending boost, and requiring the merge to
    // be finished first meant the pass itself was always made on cruise power - so
    // the bot pulled alongside a car and never got past it. Blocking is already
    // measured against BOTH the current and the destination lane a few lines up,
    // so !bBlockedAhead still means "the road I am merging into is clear", and the
    // steering limit still keeps boost off a hard corrective input.

    // Measured here rather than inferred from the charge latch, because the latch
    // reported "ready" through entire races in which boost was never applied once.
    DrivingSeconds += DeltaSeconds;
    if (bWantsBoost)
    {
        BoostActiveSeconds += DeltaSeconds;
    }

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

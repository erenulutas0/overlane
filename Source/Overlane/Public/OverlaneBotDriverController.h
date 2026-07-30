#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "Math/RandomStream.h"
#include "OverlaneBotDriverController.generated.h"

class ATrafficLanePath;
class ATrafficVehicleBase;
class AOverlaneVehiclePawn;

/**
 * The solo practice rival.
 *
 * It possesses a normal replicated vehicle pawn and drives it with the same
 * inputs a human would use, so it is bound by the same handling model. It senses
 * traffic ahead along its lane spline, follows using the traffic system's own
 * car-following curve, overtakes when it is held back, and uses turbo when the
 * road is clear.
 *
 * Server-only: race scoring and finish authority remain with the game mode, and
 * the pawn's bIsAIRacer flag keeps every human scoring path clear of it.
 */
UCLASS()
class OVERLANE_API AOverlaneBotDriverController : public AController
{
    GENERATED_BODY()

public:
    AOverlaneBotDriverController();

    void ConfigurePracticeRoute(ATrafficLanePath* InLanePath, float InStartingDistance);

    /** 0 = KOLAY, 1 = NORMAL, 2 = ZOR. Sets speed scale, turbo use and rubber band. */
    void ConfigureDifficulty(int32 Difficulty);

    /** Debug/HUD readouts. */
    float GetBotSpeedKph() const;
    float GetTrackedGapMeters() const { return LastGapToBlocker >= BigDistance ? -1.0f : LastGapToBlocker / 100.0f; }
    int32 GetCurrentLaneIndex() const { return CurrentLaneIndex; }

    /** Why the rival is or is not overtaking. Without these it is guesswork. */
    int32 GetMergesStarted() const { return MergesStarted; }
    int32 GetMergesCompleted() const { return MergesCompleted; }
    int32 GetRejectedByGain() const { return RejectedByGain; }
    int32 GetRejectedBySafety() const { return RejectedBySafety; }
    bool IsBoostEngaged() const { return bBoostEngaged; }

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    void StopVehicleInput() const;

    /** Nearest active traffic vehicle ahead of FromDistance in Lane, or null. */
    const ATrafficVehicleBase* FindNearestBlocker(
        const ATrafficLanePath* Lane, float FromDistance, float& OutGap, float& OutLeaderSpeed) const;

    void RefreshTrafficCache(float DeltaSeconds);
    void UpdateTrackedLaneDistance(const AOverlaneVehiclePawn& Vehicle, float DeltaSeconds);
    void ConsiderOvertake(const AOverlaneVehiclePawn& Vehicle, float CurrentLaneProjection, float CruiseSpeed, float BotSpeed);
    bool IsLaneChangeSafeForBot(const ATrafficLanePath* CandidateLane, const AOverlaneVehiclePawn& Vehicle, float BotSpeed) const;

    /** The shared safe-following speed law, so every caller uses one curve. */
    float ComputeFollowSpeed(float Gap, float LeaderSpeed, float CruiseSpeed) const;

    /**
     * How far the bot would get in this lane over LaneScoreHorizon seconds.
     *
     * This replaced an instantaneous "what speed does this lane allow right now"
     * score, which could not answer the question the bot actually has. The follow
     * law collapses to LeaderSpeed * Gap/MinimumFollowingDistance once inside the
     * buffer, so a car merely ALONGSIDE the bot in the candidate lane scored that
     * lane as a near-stationary wall. The director staggers lanes by 11.2 m, which
     * puts such a car there almost permanently -- so the rival rejected both
     * neighbours on "no speed gain" essentially every frame it looked.
     *
     * Rolling the same follow law forward answers it honestly: a car 3 m ahead
     * doing 45 km/h still travels 50 m in four seconds, so the lane scores as
     * "no better than mine" rather than as a wall, and a lane whose next car is
     * 60 m out scores the acceleration the bot would really get.
     */
    float ComputeLaneProjectedDistance(const ATrafficLanePath* Lane, float CruiseSpeed, float BotSpeed) const;

    /** The roll-out itself, so a hypothetical clear lane can be scored too. */
    float ProjectDistance(float Gap, float LeaderSpeed, float CruiseSpeed, float BotSpeed) const;
    float ComputeRubberBandScale(const AOverlaneVehiclePawn& Vehicle) const;

    static constexpr float BigDistance = 1.0e9f;

    /**
     * How fast the roll-out lets speed close on the follow law's target.
     *
     * Matches the throttle controller's own first-order response so a lane is
     * scored by the speed the bot could really reach in the horizon, not by the
     * speed the law would permit instantly.
     */
    static constexpr float SpeedTrackingRate = 2.0f;

    UPROPERTY(Transient)
    TObjectPtr<ATrafficLanePath> LanePath;

    /** All usable lanes, left to right. Shares the traffic director's index space. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ATrafficLanePath>> Lanes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATrafficVehicleBase>> CachedTraffic;

    // --- Following -----------------------------------------------------------
    /** Buffer held at matched speed, mirroring the traffic director's constant. */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "1.0"))
    float MinimumFollowingDistance = 1200.0f;

    /**
     * The deceleration the follow law plans around, well below the pawn's
     * BrakingDeceleration of 7200 cm/s^2 so there is headroom for a surprise.
     *
     * This replaced a scaled-threshold law whose window was derived from the
     * bot's own instantaneous speed. That was a feedback oscillator: faster bot
     * -> wider window -> follow engages -> brake -> narrower window -> follow
     * disengages -> full throttle, cycling forever. The law below is continuous
     * and monotonic in gap, so it has no threshold to cross.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "1440.0"))
    float ComfortDeceleration = 1600.0f;

    /**
     * The deceleration a MERGE is planned around, as opposed to cruising.
     *
     * These are two different questions and sharing one number for both was a
     * structural lockout. ComfortDeceleration is how gently the bot wants to trail
     * a car it has decided to sit behind. Threading a gap is not that: a racer
     * commits to the gap and then brakes hard. Planning the merge at 1600 cm/s^2
     * demanded 78 m of clear road in the target lane at a 4750 cm/s closing speed,
     * against traffic the director packs at 35 m - so the rival rejected 6682
     * merges on safety in a single run while completing 8.
     *
     * Two thirds of the pawn's own BrakingDeceleration of 7200, so the authorised
     * merge always has 1.5x more stopping power in reserve than it assumed. The
     * follow law still plans at the comfort figure once settled, which is why the
     * bot brakes to 1.0 immediately after taking a tight gap - that is intended,
     * and it is the same saturation the EmergencyGap branch relies on.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "1600.0"))
    float MergeDeceleration = 4800.0f;

    /** Bot half-length plus the largest traffic half-length, rounded up. */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "0.0"))
    float BumperAllowance = 520.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "0.0"))
    float EmergencyGap = 600.0f;

    // --- Steering ------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.0"))
    float LookAheadTimeSeconds = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "100.0"))
    float MinLookAheadDistance = 900.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "100.0"))
    float MaxLookAheadDistance = 3200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.0"))
    float SteeringProportionalGain = 1.9f;

    /**
     * Defaults to zero, deliberately.
     *
     * The obvious "damping" term -Kd * d(HeadingError)/dt is ANTI-damping here:
     * HeadingError contains the vehicle's own heading, so differentiating it
     * feeds the yaw rate back with a positive sign. Measured closed-loop damping
     * ratio is 0.489 at Kd = 0 and 0.415 at Kd = 0.28, i.e. the term made the
     * merge overshoot worse (141 uu instead of 100 uu of lane-width overshoot).
     * If real damping is ever wanted it must be taken from measured yaw rate,
     * not from the error derivative, and the proportional gain re-tuned with it.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.0"))
    float SteeringDampingGain = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.1"))
    float SteeringSlewRate = 4.0f;

    /** A merge is only complete once the outward yaw is shed, not just the offset. */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.1"))
    float LaneCompleteHeadingToleranceDegrees = 3.5f;

    // --- Overtaking ----------------------------------------------------------
    /**
     * How long the rival must be held below cruise before it looks for a way out.
     *
     * With the old 0.6 s plus a 2.0 s post-merge cooldown and a ~1.6 s merge, one
     * overtake cost at least 4.2 s - a hard ceiling of about 27 passes over a
     * two-minute route, and the rival was measured completing 20. The binding
     * constraint had moved from "cannot merge" to "cannot merge often enough".
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float BlockedTimeToOvertake = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float PostMergeCooldown = 0.9f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float AbortedMergeCooldown = 1.2f;

    /**
     * How far ahead the follow law is rolled forward when scoring a lane.
     *
     * Long enough that a clear lane shows its acceleration (the bot needs about
     * three seconds to climb from traffic speed to cruise), short enough that the
     * roll-out is not predicting traffic that will have moved by then.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.5"))
    float LaneScoreHorizon = 4.0f;

    /**
     * Extra ground the candidate lane must cover over LaneScoreHorizon, in cm.
     *
     * This is the third currency tried here, and the first that is comparable
     * between lanes. A raw-gap margin failed because every lane is pinned to the
     * director's platoon spacing; a cm/s margin failed because the follow law's
     * fixed point makes the bot an exact speed copy of its leader, so the current
     * lane's score and the candidate's were both just "whatever traffic is doing".
     * Distance over a horizon separates them: it integrates the acceleration the
     * bot would actually get, which is the whole point of changing lane.
     *
     * 500 cm over four seconds is about 4.5 km/h of average gain - small enough
     * that ordinary gaps qualify, large enough that identical lanes do not.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float OvertakeDistanceGain = 500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float LaneChangeEndBuffer = 2000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.1"))
    float LaneChangeTimeout = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float LaneCompleteTolerance = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float RivalPawnClearance = 700.0f;

    /**
     * Room required ahead and behind AT THE MOMENT OF MERGING, on top of the
     * bumper allowance and the closing-speed braking term.
     *
     * Deliberately much smaller than MinimumFollowingDistance. That value is the
     * settled gap the follow law converges to, and requiring it during a merge
     * made the window wider than the gaps the traffic director actually creates -
     * so the rival could essentially never change lane and sat behind whatever
     * was in front of it for the rest of the race.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float MergeBufferAhead = 250.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float MergeBufferBehind = 150.0f;

    // --- Difficulty ----------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Difficulty", meta = (ClampMin = "0.0"))
    float RubberBandRangeMeters = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BoostEngageCharge = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BoostReleaseCharge = 0.08f;

    /** Rubber banding is frozen inside this distance so the finish is honest. */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Difficulty", meta = (ClampMin = "0.0"))
    float RubberBandFreezeDistance = 30000.0f;

    float DifficultySpeedScale = 1.0f;
    float RubberBandStrength = 0.06f;
    bool bAllowBoost = true;

    FRandomStream RaceStream;

    // --- Runtime state -------------------------------------------------------
    float StartingDistance = 0.0f;
    float TrackedLaneDistance = -1.0f;
    float PreviousHeadingError = 0.0f;
    float SmoothedSteering = 0.0f;
    float BlockedSeconds = 0.0f;
    float LaneChangeElapsed = 0.0f;
    float LaneChangeCooldown = 1.5f;
    float TrafficCacheRemaining = 0.0f;
    float RivalContactCooldown = 0.0f;
    float LastGapToBlocker = BigDistance;
    int32 CurrentLaneIndex = INDEX_NONE;
    int32 TargetLaneIndex = INDEX_NONE;
    int32 SteeringTargetLaneIndex = INDEX_NONE;
    int32 MergesStarted = 0;
    int32 MergesCompleted = 0;
    int32 RejectedByGain = 0;
    int32 RejectedBySafety = 0;
    bool bBlockedAhead = false;
    bool bBoostEngaged = false;
    bool bWasRivalContactActive = false;
};

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
    void ConsiderOvertake(const AOverlaneVehiclePawn& Vehicle, float CurrentSpeedPotential, float CruiseSpeed, float BotSpeed);
    bool IsLaneChangeSafeForBot(const ATrafficLanePath* CandidateLane, const AOverlaneVehiclePawn& Vehicle, float BotSpeed) const;

    /** The shared safe-following speed law, so every caller uses one curve. */
    float ComputeFollowSpeed(float Gap, float LeaderSpeed, float CruiseSpeed) const;

    /** The speed this lane would actually allow right now. */
    float ComputeLaneSpeedPotential(const ATrafficLanePath* Lane, float CruiseSpeed) const;
    float ComputeRubberBandScale(const AOverlaneVehiclePawn& Vehicle) const;

    static constexpr float BigDistance = 1.0e9f;

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
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float BlockedTimeToOvertake = 0.6f;

    /**
     * How much faster the candidate lane must let the bot go, in cm/s.
     *
     * This replaced a raw-gap margin. Gap is the wrong currency: the traffic
     * director packs each lane into platoons spaced at its own following
     * distance, so "candidate gap must beat my gap by 15 m" was comparing two
     * numbers that are both pinned to that spacing, and essentially never passed.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float OvertakeSpeedGain = 450.0f;

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

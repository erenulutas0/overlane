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

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    void StopVehicleInput() const;

    /** Nearest active traffic vehicle ahead of FromDistance in Lane, or null. */
    const ATrafficVehicleBase* FindNearestBlocker(
        const ATrafficLanePath* Lane, float FromDistance, float& OutGap, float& OutLeaderSpeed) const;

    void RefreshTrafficCache(float DeltaSeconds);
    void UpdateTrackedLaneDistance(const AOverlaneVehiclePawn& Vehicle, float DeltaSeconds);
    void ConsiderOvertake(const AOverlaneVehiclePawn& Vehicle, float CurrentGap, float BotSpeed);
    bool IsLaneChangeSafeForBot(const ATrafficLanePath* CandidateLane, const AOverlaneVehiclePawn& Vehicle, float BotSpeed) const;
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
    // The two distances mirror ATrafficDirector's own constants so the rival and
    // the traffic it drives among behave consistently.
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "0.0"))
    float FollowingDistance = 3200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "0.0"))
    float MinimumFollowingDistance = 1200.0f;

    /**
     * Traffic closes on traffic at ~1050 cm/s, but the bot closes at up to
     * 3950 cm/s, so the raw traffic distances give it under a second of warning.
     * The distances are scaled by real closing speed, keeping the curve shape.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Following", meta = (ClampMin = "1.0"))
    float ReferenceClosingSpeed = 1050.0f;

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

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.0"))
    float SteeringDampingGain = 0.28f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Steering", meta = (ClampMin = "0.1"))
    float SteeringSlewRate = 4.0f;

    // --- Overtaking ----------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float BlockedTimeToOvertake = 0.6f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float OvertakeGainMargin = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float LaneChangeEndBuffer = 2000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.1"))
    float LaneChangeTimeout = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float LaneCompleteTolerance = 90.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot|Overtake", meta = (ClampMin = "0.0"))
    float RivalPawnClearance = 700.0f;

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
    bool bBlockedAhead = false;
    bool bBoostEngaged = false;
    bool bWasRivalContactActive = false;
};

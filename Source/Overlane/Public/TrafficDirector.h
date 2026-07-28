#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficDirector.generated.h"

class ATrafficLanePath;
class ATrafficVehicleBase;

/** Local-only traffic bootstrapper with a small reusable vehicle pool. */
UCLASS()
class OVERLANE_API ATrafficDirector : public AActor
{
    GENERATED_BODY()

public:
    ATrafficDirector();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    int32 GetActiveVehicleCount() const;
    int32 GetPoolSize() const { return VehiclePool.Num(); }

    /**
     * The shared car-following curve: speed ramps linearly from a full stop at
     * MinGap up to the leader's own speed at MaxGap. Exposed as a static so the
     * AI rival follows traffic using exactly the same law traffic follows itself
     * with, rather than a second hand-tuned approximation.
     */
    static float ComputeFollowSpeedLimit(float LeaderSpeed, float Gap, float MinGap, float MaxGap);

private:
    void SpawnInitialPool();
    void ActivateVehicle(int32 VehicleIndex);
    void RefreshSpawnDistance(int32 VehicleIndex);
    void RecycleVehiclesBehindPlayer();
    void RefreshRacerCache(float DeltaSeconds);

    /**
     * Furthest racer along the lane, humans and the AI rival alike. Drives the
     * spawn window: anchoring on humans only let a rival 90 m ahead outrun the
     * window entirely and drive on empty road.
     */
    float GetLeadRacerLaneDistance(const ATrafficLanePath* Lane) const;

    /**
     * Nearest racer along the lane. Drives recycling, which must use the
     * trailing racer or it despawns cars the human behind still needs.
     */
    float GetTrailingRacerLaneDistance(const ATrafficLanePath* Lane) const;
    bool IsSpawnSafeForPlayer(int32 VehicleIndex) const;
    bool IsSpawnSafeForTraffic(int32 VehicleIndex) const;
    void UpdateTrafficFollowing(float DeltaSeconds);
    void TryStartGuardedLaneChange();
    bool IsLaneChangeSafe(const ATrafficVehicleBase* Vehicle, const ATrafficLanePath* TargetLane) const;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "0.0"))
    float InitialSpawnDistance = 12000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "1", ClampMax = "12"))
    int32 VehiclesPerLane = 7;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "0.0"))
    float TrafficSpacing = 3500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "0.0"))
    float RespawnDelaySeconds = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "0.0"))
    float MinimumPlayerSpawnDistance = 12000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes", meta = (ClampMin = "0.1"))
    float LaneChangeDuration = 2.4f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes")
    bool bEnableLaneChanges = true;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes", meta = (ClampMin = "0.0"))
    float LaneChangeInterval = 2.75f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes", meta = (ClampMin = "0.0"))
    float LaneChangeEndBuffer = 2000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes", meta = (ClampMin = "0.0"))
    float MinimumPlayerLaneChangeDistance = 6000.0f;

    /**
     * Smaller than the human exclusion on purpose. The human deserves a wide
     * no-merge bubble for fairness; the rival only needs physical clearance,
     * and a 60 m bubble around it would suppress most traffic lane changes.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes", meta = (ClampMin = "0.0"))
    float MinimumBotLaneChangeDistance = 2500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Lane Changes", meta = (ClampMin = "0.0"))
    float TargetLaneClearance = 2800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Following", meta = (ClampMin = "0.0"))
    float FollowingDistance = 3200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Following", meta = (ClampMin = "0.0"))
    float MinimumFollowingDistance = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Spawning", meta = (ClampMin = "0.0"))
    float MinimumTrafficSpawnDistance = 3200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic|Spawning", meta = (ClampMin = "0.0"))
	float RecycleBehindPlayerDistance = 15000.0f;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATrafficLanePath>> Lanes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATrafficVehicleBase>> VehiclePool;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATrafficLanePath>> AvailableLanes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<class AOverlaneVehiclePawn>> CachedRacers;

    float RacerCacheRemaining = 0.0f;

    TArray<float> PoolSpawnDistances;
    TArray<float> RespawnTimers;
    float LaneChangeCooldownRemaining = 1.5f;
    int32 LaneChangeAttemptCounter = 0;
};

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
     * Draws traffic lane-change directions from an owned stream instead of the global
     * RNG, so pinning overlane.Bot.Seed makes a whole race reproducible. With the
     * global RNG, which way a middle-lane car jinked differed run to run and moved the
     * very traffic field the rival was being measured against.
     */
    FRandomStream LaneChangeStream;

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

    float GetRacerLaneDistance(int32 RacerIndex, const ATrafficLanePath* Lane) const;

    /**
     * Which racer this pool slot should supply next.
     *
     * Traffic used to spawn ahead of the FURTHEST racer and recycle behind the
     * NEAREST one. With a leader running at 130 km/h and a trailing racer moving
     * at traffic speed, nothing ever fell far enough behind to recycle, so
     * nothing respawned: the leader punched through the pack in the first minute
     * and drove the remaining five kilometres on a completely empty road, while
     * the trailing racer stayed buried in a jam it could never clear.
     *
     * Each pool slot is now anchored to whichever racer currently has the least
     * traffic in front of it, so density is maintained around every racer
     * independently and neither position is privileged.
     */
    int32 PickAnchorRacerForSpawn(const ATrafficLanePath* Lane) const;

    /**
     * Pool entries per lane. Every place that maps a pool index to a lane or a
     * slot must use this, not VehiclesPerLane: the pool block per lane grew when
     * traffic started being shared between racers.
     */
    int32 GetSlotsPerLane() const { return VehiclesPerLane * FMath::Max(1, RacerSupplyCapacity); }
    bool IsSpawnSafeForPlayer(int32 VehicleIndex) const;
    bool IsSpawnSafeForTraffic(int32 VehicleIndex) const;
    void UpdateTrafficFollowing(float DeltaSeconds);
    void TryStartGuardedLaneChange();
    bool IsLaneChangeSafe(const ATrafficVehicleBase* Vehicle, const ATrafficLanePath* TargetLane) const;

    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "0.0"))
    float InitialSpawnDistance = 12000.0f;

    /** Cars per lane, PER RACER. The pool is this times lanes times capacity. */
    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "1", ClampMax = "12"))
    int32 VehiclesPerLane = 7;

    /**
     * How many racers the pool is sized to keep supplied at once. Traffic is
     * shared between them, so a race with a rival needs twice the pool of a
     * time trial to feel the same density.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Traffic", meta = (ClampMin = "1", ClampMax = "4"))
    int32 RacerSupplyCapacity = 2;

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

    /** Index into CachedRacers that each pool slot is currently supplying. */
    TArray<int32> PoolAnchorRacer;

    /** Slot within its racer's supply, used for spacing along the lane. */
    TArray<int32> PoolSlotIndex;

    TArray<float> PoolSpawnDistances;
    TArray<float> RespawnTimers;
    float LaneChangeCooldownRemaining = 1.5f;
    int32 LaneChangeAttemptCounter = 0;
};

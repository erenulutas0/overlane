#include "TrafficDirector.h"

#include "Kismet/GameplayStatics.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneVehiclePawn.h"
#include "TrafficLanePath.h"
#include "TrafficVehicleBase.h"
#include "GameFramework/PlayerController.h"

ATrafficDirector::ATrafficDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorHiddenInGame(true);
}

void ATrafficDirector::BeginPlay()
{
    Super::BeginPlay();

    // This actor is created by the authoritative GameMode. Only the server
    // creates and drives the reusable pool; individual traffic actors
    // replicate their visual state and transforms to connected players.
    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
        return;
    }

    if (const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
        GameMode && GameMode->IsMultiplayerRace())
    {
        // First network traffic pass: six cars, substantial gaps, and no lane
        // changes. This validates shared visibility and collision before the
        // more complex local lane-change model is networked.
        VehiclesPerLane = 2;
        InitialSpawnDistance = 18000.0f;
        TrafficSpacing = 8500.0f;
        MinimumPlayerSpawnDistance = 10000.0f;
        bEnableLaneChanges = false;
    }

    SpawnInitialPool();
}

void ATrafficDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        return;
    }

    const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (!GameMode || !GameMode->IsSimulationActive())
    {
        return;
    }

    // Must run before anything that asks where the racers are.
    RefreshRacerCache(DeltaSeconds);

    RecycleVehiclesBehindPlayer();

    UpdateTrafficFollowing(DeltaSeconds);

    if (bEnableLaneChanges)
    {
        LaneChangeCooldownRemaining -= DeltaSeconds;
        if (LaneChangeCooldownRemaining <= 0.0f)
        {
            TryStartGuardedLaneChange();
            LaneChangeCooldownRemaining = LaneChangeInterval;
        }
    }

    for (int32 Index = 0; Index < VehiclePool.Num(); ++Index)
    {
        ATrafficVehicleBase* Vehicle = VehiclePool[Index];
        if (!Vehicle || Vehicle->IsTrafficActive())
        {
            continue;
        }

        RefreshSpawnDistance(Index);
        RespawnTimers[Index] -= DeltaSeconds;
        if (RespawnTimers[Index] <= 0.0f && IsSpawnSafeForPlayer(Index) && IsSpawnSafeForTraffic(Index))
        {
            ActivateVehicle(Index);
        }
    }
}

int32 ATrafficDirector::GetActiveVehicleCount() const
{
    int32 ActiveCount = 0;
    for (const ATrafficVehicleBase* Vehicle : VehiclePool)
    {
        ActiveCount += Vehicle && Vehicle->IsTrafficActive() ? 1 : 0;
    }

    return ActiveCount;
}

void ATrafficDirector::SpawnInitialPool()
{
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(this, ATrafficLanePath::StaticClass(), FoundActors);
    FoundActors.Sort([](const AActor& Left, const AActor& Right)
    {
        return Left.GetActorLocation().Y < Right.GetActorLocation().Y;
    });

    for (int32 LaneIndex = 0; LaneIndex < FoundActors.Num(); ++LaneIndex)
    {
        AActor* FoundActor = FoundActors[LaneIndex];
        ATrafficLanePath* Lane = Cast<ATrafficLanePath>(FoundActor);
        if (!Lane || Lane->GetLaneLength() <= 100.0f)
        {
            continue;
        }

        AvailableLanes.Add(Lane);

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // The pool is sized for every racer it may have to supply, not just one:
        // traffic is now shared between racers rather than anchored to whoever
        // happens to be in front.
        const int32 SlotsPerLane = GetSlotsPerLane();
        for (int32 SlotIndex = 0; SlotIndex < SlotsPerLane; ++SlotIndex)
        {
            ATrafficVehicleBase* Vehicle = GetWorld()->SpawnActor<ATrafficVehicleBase>(
                ATrafficVehicleBase::StaticClass(), Lane->GetTransformAtDistance(0.0f), SpawnParameters);

            if (Vehicle)
            {
                Lanes.Add(Lane);
                VehiclePool.Add(Vehicle);
                PoolSpawnDistances.Add(0.0f);
                RespawnTimers.Add(0.0f);
                PoolAnchorRacer.Add(0);
                PoolSlotIndex.Add(SlotIndex % VehiclesPerLane);
                RefreshSpawnDistance(VehiclePool.Num() - 1);
            }
        }
    }
}

void ATrafficDirector::RefreshRacerCache(float DeltaSeconds)
{
    RacerCacheRemaining -= DeltaSeconds;
    if (RacerCacheRemaining > 0.0f && CachedRacers.Num() > 0)
    {
        return;
    }

    // These lookups run per traffic vehicle per frame, so the actor scan is
    // amortised rather than repeated.
    RacerCacheRemaining = 0.5f;

    TArray<AActor*> FoundRacers;
    UGameplayStatics::GetAllActorsOfClass(this, AOverlaneVehiclePawn::StaticClass(), FoundRacers);

    CachedRacers.Reset();
    for (AActor* RacerActor : FoundRacers)
    {
        if (AOverlaneVehiclePawn* Racer = Cast<AOverlaneVehiclePawn>(RacerActor))
        {
            CachedRacers.Add(Racer);
        }
    }
}

float ATrafficDirector::GetRacerLaneDistance(int32 RacerIndex, const ATrafficLanePath* Lane) const
{
    if (!Lane || !CachedRacers.IsValidIndex(RacerIndex) || !CachedRacers[RacerIndex])
    {
        return 0.0f;
    }

    return Lane->GetClosestDistanceToLocation(CachedRacers[RacerIndex]->GetActorLocation());
}

int32 ATrafficDirector::PickAnchorRacerForSpawn(const ATrafficLanePath* Lane) const
{
    if (CachedRacers.Num() <= 1 || !Lane)
    {
        return 0;
    }

    // The band a racer can actually see filling up ahead of it.
    const float SupplyBand = InitialSpawnDistance + (VehiclesPerLane * TrafficSpacing);

    int32 BestRacer = 0;
    int32 BestCount = TNumericLimits<int32>::Max();

    for (int32 RacerIndex = 0; RacerIndex < CachedRacers.Num(); ++RacerIndex)
    {
        if (!CachedRacers[RacerIndex])
        {
            continue;
        }

        const float RacerDistance = GetRacerLaneDistance(RacerIndex, Lane);

        int32 SuppliedCount = 0;
        for (int32 PoolIndex = 0; PoolIndex < VehiclePool.Num(); ++PoolIndex)
        {
            const ATrafficVehicleBase* Vehicle = VehiclePool[PoolIndex];
            if (!Vehicle || !Vehicle->IsTrafficActive() || Lanes[PoolIndex] != Lane)
            {
                continue;
            }

            const float Relative = Vehicle->GetLaneDistance() - RacerDistance;
            if (Relative > -RecycleBehindPlayerDistance && Relative < SupplyBand)
            {
                ++SuppliedCount;
            }
        }

        if (SuppliedCount < BestCount)
        {
            BestCount = SuppliedCount;
            BestRacer = RacerIndex;
        }
    }

    return BestRacer;
}

void ATrafficDirector::RefreshSpawnDistance(int32 VehicleIndex)
{
    if (!Lanes.IsValidIndex(VehicleIndex) || !PoolSpawnDistances.IsValidIndex(VehicleIndex))
    {
        return;
    }

    const ATrafficLanePath* Lane = Lanes[VehicleIndex];
    if (!Lane)
    {
        return;
    }

    // Choose which racer this slot supplies, and remember it: recycling has to
    // use the SAME racer, or a car spawned for one racer would be judged stale
    // against another and the pool would thrash.
    const int32 AnchorRacer = PickAnchorRacerForSpawn(Lane);
    if (PoolAnchorRacer.IsValidIndex(VehicleIndex))
    {
        PoolAnchorRacer[VehicleIndex] = AnchorRacer;
    }

    const int32 SlotIndex = PoolSlotIndex.IsValidIndex(VehicleIndex) ? PoolSlotIndex[VehicleIndex] : 0;
    const int32 LaneIndex = AvailableLanes.IndexOfByKey(Lane);
    const float LaneStagger = (FMath::Max(0, LaneIndex) % 3) * (TrafficSpacing * 0.32f);
    const float RequestedDistance = GetRacerLaneDistance(AnchorRacer, Lane)
        + InitialSpawnDistance + (SlotIndex * TrafficSpacing) + LaneStagger;
    PoolSpawnDistances[VehicleIndex] = FMath::Min(RequestedDistance, Lane->GetLaneLength() - 100.0f);
}

void ATrafficDirector::RecycleVehiclesBehindPlayer()
{
    for (int32 Index = 0; Index < VehiclePool.Num(); ++Index)
    {
        ATrafficVehicleBase* Vehicle = VehiclePool[Index];
        ATrafficLanePath* Lane = Lanes.IsValidIndex(Index) ? Lanes[Index] : nullptr;
        if (!Vehicle || !Lane || !Vehicle->IsTrafficActive())
        {
            continue;
        }

        // Judged against the racer this car was spawned to supply, not against
        // whoever happens to be furthest back. Anchoring recycling on the
        // trailing racer meant that once a leader pulled away, nothing behind
        // them ever aged out and nothing new ever spawned.
        const int32 AnchorRacer = PoolAnchorRacer.IsValidIndex(Index) ? PoolAnchorRacer[Index] : 0;
        const float AnchorDistance = GetRacerLaneDistance(AnchorRacer, Lane);
        const float Relative = Vehicle->GetLaneDistance() - AnchorDistance;

        // Behind the anchor: the normal exit.
        bool bShouldRecycle = Relative < -RecycleBehindPlayerDistance;

        // FORWARD exit, and it is not optional.
        //
        // Recycling only ever fired behind the anchor. A racer travelling at
        // traffic speed never gets 150 m past anything, and any car FASTER than
        // it drifts beyond the supply band and can never come back - the pool
        // slot leaks for the rest of the race. Worse, a queue that forms in front
        // of a slow racer can never clear, which is how a rival ended up averaging
        // 12.8 km/h for 77 seconds: slower than the slowest traffic on the road.
        const float SupplyBand = InitialSpawnDistance + (VehiclesPerLane * TrafficSpacing);
        bShouldRecycle |= Relative > SupplyBand + RecycleBehindPlayerDistance;

        if (bShouldRecycle)
        {
            Vehicle->DeactivateForPool();
            RespawnTimers[Index] = RespawnDelaySeconds;
        }
    }
}

void ATrafficDirector::ActivateVehicle(int32 VehicleIndex)
{
    if (!VehiclePool.IsValidIndex(VehicleIndex) || !Lanes.IsValidIndex(VehicleIndex))
    {
        return;
    }

    ATrafficVehicleBase* Vehicle = VehiclePool[VehicleIndex];
    ATrafficLanePath* Lane = Lanes[VehicleIndex];
    if (!Vehicle || !Lane)
    {
        return;
    }

    const float SpawnDistance = PoolSpawnDistances[VehicleIndex];
    struct FTrafficProfile
    {
        FLinearColor Color;
        float Speed;
        FName Name;
        FVector MeshScale;
        FVector CollisionExtent;
    };

    // Highway speeds, in cm/s. The previous set ran 38-76 km/h, which is urban
    // traffic, and it was doing far more damage than it looked.
    //
    // The rival's follow law makes it an exact speed copy of whatever it settles
    // behind, so a 50 km/h Commuter capped the rival at 50 km/h against a player
    // doing 245 - it read as "the bot drives like a traffic car" because it
    // literally was one. Three rounds of work on the overtake logic could not fix
    // that, because the ceiling was never in the overtake logic.
    //
    // It also drove the merge windows. Clearance scales with the SQUARE of closing
    // speed, so cutting closing from ~195 km/h to ~115 km/h takes the gap the
    // rival needs from 31 m to about 18 m, against a director that packs lanes at
    // 35 m. The same change that makes the road read as a motorway is the one that
    // makes overtaking geometrically possible.
    //
    // Truck stays slowest at 83 km/h: heavy vehicles really are speed-limited, so
    // it is both the honest choice and the thing that keeps a reason to overtake.
    // The Commuter/Coupe/Sport ordering is preserved because slot order depends on
    // it for initial same-lane spacing.
    static const FTrafficProfile TrafficProfiles[] =
    {
        { FLinearColor(1.0f, 0.28f, 0.16f), 2800.0f, TEXT("Commuter"), FVector(4.2f, 1.9f, 1.3f), FVector(210.0f, 95.0f, 65.0f) },
        { FLinearColor(0.15f, 0.55f, 1.0f), 3200.0f, TEXT("Coupe"), FVector(3.8f, 1.75f, 1.05f), FVector(190.0f, 88.0f, 55.0f) },
        { FLinearColor(1.0f, 0.78f, 0.12f), 3600.0f, TEXT("Sport"), FVector(4.5f, 1.85f, 0.95f), FVector(225.0f, 92.0f, 52.0f) },
        { FLinearColor(0.22f, 0.72f, 0.38f), 3000.0f, TEXT("SUV"), FVector(4.6f, 2.1f, 1.55f), FVector(230.0f, 105.0f, 82.0f) },
        { FLinearColor(0.35f, 0.75f, 0.42f), 2300.0f, TEXT("Truck"), FVector(6.5f, 2.15f, 2.1f), FVector(325.0f, 108.0f, 105.0f) }
    };

    const int32 SlotIndex = VehicleIndex % GetSlotsPerLane();
    const int32 LaneIndex = VehicleIndex / GetSlotsPerLane();
    // Keep a single slow truck near the far end of the whole pool. Repeating trucks
    // in every lane made the early test road congest before lane changes could occur.
    // Offset the profile cycle per lane so even the short multiplayer pool gets
    // a visible SUV without relying on non-deterministic random selection.
    const int32 ProfileIndex = VehicleIndex == VehiclePool.Num() - 1
        ? 4
        : (SlotIndex + (LaneIndex * 2)) % 4;
    const FTrafficProfile& Profile = TrafficProfiles[ProfileIndex];
    Vehicle->SetTrafficColor(Profile.Color);
    Vehicle->SetTrafficVariant(Profile.Name, Profile.MeshScale, Profile.CollisionExtent);
    Vehicle->ActivateOnLane(Lane, SpawnDistance, Profile.Speed);
    RespawnTimers[VehicleIndex] = RespawnDelaySeconds;
}

bool ATrafficDirector::IsSpawnSafeForPlayer(int32 VehicleIndex) const
{
    if (!Lanes.IsValidIndex(VehicleIndex) || !PoolSpawnDistances.IsValidIndex(VehicleIndex))
    {
        return false;
    }

    // Every racer counts, not just human ones: traffic used to spawn inside the
    // AI rival because it has no player controller to iterate.
    const FVector SpawnLocation = Lanes[VehicleIndex]->GetTransformAtDistance(PoolSpawnDistances[VehicleIndex]).GetLocation();
    for (const AOverlaneVehiclePawn* Racer : CachedRacers)
    {
        if (Racer && FVector::DistSquared2D(Racer->GetActorLocation(), SpawnLocation) < FMath::Square(MinimumPlayerSpawnDistance))
        {
            return false;
        }
    }

    return true;
}

bool ATrafficDirector::IsSpawnSafeForTraffic(int32 VehicleIndex) const
{
    if (!Lanes.IsValidIndex(VehicleIndex) || !PoolSpawnDistances.IsValidIndex(VehicleIndex))
    {
        return false;
    }

    const ATrafficLanePath* SpawnLane = Lanes[VehicleIndex];
    const float SpawnDistance = PoolSpawnDistances[VehicleIndex];
    for (const ATrafficVehicleBase* OtherVehicle : VehiclePool)
    {
        if (!OtherVehicle || !OtherVehicle->IsTrafficActive() || !OtherVehicle->IsOccupyingLane(SpawnLane))
        {
            continue;
        }

        if (FMath::Abs(OtherVehicle->GetLaneDistance() - SpawnDistance) < MinimumTrafficSpawnDistance)
        {
            return false;
        }
    }

    return true;
}

float ATrafficDirector::ComputeFollowSpeedLimit(float LeaderSpeed, float Gap, float MinGap, float MaxGap)
{
    const float GapAlpha = FMath::GetRangePct(MinGap, MaxGap, Gap);
    return LeaderSpeed * FMath::Clamp(GapAlpha, 0.0f, 1.0f);
}

void ATrafficDirector::UpdateTrafficFollowing(float DeltaSeconds)
{
    for (ATrafficVehicleBase* Vehicle : VehiclePool)
    {
        if (!Vehicle || !Vehicle->IsTrafficActive())
        {
            continue;
        }

        float SpeedLimit = Vehicle->GetDesiredSpeed();
        for (const ATrafficLanePath* OccupiedLane : AvailableLanes)
        {
            if (!OccupiedLane || !Vehicle->IsOccupyingLane(OccupiedLane))
            {
                continue;
            }

            const ATrafficVehicleBase* VehicleAhead = nullptr;
            float ClosestGap = TNumericLimits<float>::Max();
            for (const ATrafficVehicleBase* OtherVehicle : VehiclePool)
            {
                if (!OtherVehicle || OtherVehicle == Vehicle || !OtherVehicle->IsTrafficActive() || !OtherVehicle->IsOccupyingLane(OccupiedLane))
                {
                    continue;
                }

                const float Gap = OtherVehicle->GetLaneDistance() - Vehicle->GetLaneDistance();
                if (Gap > 0.0f && Gap < ClosestGap)
                {
                    ClosestGap = Gap;
                    VehicleAhead = OtherVehicle;
                }
            }

            if (VehicleAhead && ClosestGap < FollowingDistance)
            {
                SpeedLimit = FMath::Min(
                    SpeedLimit,
                    ComputeFollowSpeedLimit(VehicleAhead->GetCurrentSpeed(), ClosestGap, MinimumFollowingDistance, FollowingDistance));
            }
        }

        Vehicle->SetTrafficSpeedLimit(SpeedLimit);
    }
}

void ATrafficDirector::TryStartGuardedLaneChange()
{
    if (AvailableLanes.Num() < 2 || VehiclePool.IsEmpty())
    {
        return;
    }

    // Try fast profiles first, then medium, then slow. This makes profile behavior
    // observable before faster cars reach the end of the current test road.
    static const int32 SlotPriority[] = { 5, 3, 0, 6, 4, 1, 2 };
    const int32 PriorityIndex = LaneChangeAttemptCounter % UE_ARRAY_COUNT(SlotPriority);
    const int32 RequestedSlot = SlotPriority[PriorityIndex] % GetSlotsPerLane();
    const int32 FirstLaneIndex = (LaneChangeAttemptCounter / UE_ARRAY_COUNT(SlotPriority)) % AvailableLanes.Num();
    ++LaneChangeAttemptCounter;

    for (int32 LaneOffset = 0; LaneOffset < AvailableLanes.Num(); ++LaneOffset)
    {
        const int32 LaneIndex = (FirstLaneIndex + LaneOffset) % AvailableLanes.Num();
        const int32 VehicleIndex = (LaneIndex * GetSlotsPerLane()) + RequestedSlot;
        if (!VehiclePool.IsValidIndex(VehicleIndex))
        {
            continue;
        }

        ATrafficVehicleBase* Vehicle = VehiclePool[VehicleIndex];
        if (!Vehicle || !Vehicle->IsTrafficActive() || Vehicle->IsChangingLane() || Vehicle->GetCurrentSpeed() > Vehicle->GetDesiredSpeed() * 0.82f)
        {
            continue;
        }

        const int32 CurrentLaneIndex = AvailableLanes.IndexOfByKey(Vehicle->GetAssignedLane());
        if (CurrentLaneIndex == INDEX_NONE || Vehicle->GetLaneDistance() > Vehicle->GetAssignedLane()->GetLaneLength() - LaneChangeEndBuffer)
        {
            continue;
        }

        const int32 Direction = CurrentLaneIndex == 0 ? 1 : (CurrentLaneIndex == AvailableLanes.Num() - 1 ? -1 : (FMath::RandBool() ? -1 : 1));
        ATrafficLanePath* TargetLane = AvailableLanes[CurrentLaneIndex + Direction];
        if (IsLaneChangeSafe(Vehicle, TargetLane) && Vehicle->BeginLaneChange(TargetLane, LaneChangeDuration))
        {
            return;
        }
    }
}

bool ATrafficDirector::IsLaneChangeSafe(const ATrafficVehicleBase* Vehicle, const ATrafficLanePath* TargetLane) const
{
    if (!Vehicle || !TargetLane)
    {
        return false;
    }

    const FVector TargetLocation = TargetLane->GetTransformAtDistance(Vehicle->GetLaneDistance()).GetLocation();
    for (const AOverlaneVehiclePawn* Racer : CachedRacers)
    {
        if (!Racer)
        {
            continue;
        }

        // The human gets a wide fairness bubble; the rival only needs physical
        // clearance, or traffic would stop changing lanes anywhere near it.
        const float Exclusion = Racer->IsAIRacer() ? MinimumBotLaneChangeDistance : MinimumPlayerLaneChangeDistance;
        if (FVector::DistSquared2D(Racer->GetActorLocation(), TargetLocation) < FMath::Square(Exclusion))
        {
            return false;
        }
    }

    for (const ATrafficVehicleBase* OtherVehicle : VehiclePool)
    {
        if (OtherVehicle && OtherVehicle != Vehicle && OtherVehicle->IsOccupyingLane(TargetLane) && FMath::Abs(OtherVehicle->GetLaneDistance() - Vehicle->GetLaneDistance()) < TargetLaneClearance)
        {
            return false;
        }
    }

    return true;
}

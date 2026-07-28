#include "TrafficDirector.h"

#include "Kismet/GameplayStatics.h"
#include "OverlaneGameModeBase.h"
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
        for (int32 SlotIndex = 0; SlotIndex < VehiclesPerLane; ++SlotIndex)
        {
            ATrafficVehicleBase* Vehicle = GetWorld()->SpawnActor<ATrafficVehicleBase>(
                ATrafficVehicleBase::StaticClass(), Lane->GetTransformAtDistance(0.0f), SpawnParameters);

            if (Vehicle)
            {
                Lanes.Add(Lane);
                VehiclePool.Add(Vehicle);
                PoolSpawnDistances.Add(0.0f);
                RespawnTimers.Add(0.0f);
                RefreshSpawnDistance(VehiclePool.Num() - 1);
            }
        }
    }
}

float ATrafficDirector::GetPlayerLaneDistance(const ATrafficLanePath* Lane) const
{
    if (!Lane || !GetWorld())
    {
        return 0.0f;
    }

    float FurthestPlayerDistance = 0.0f;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APawn* PlayerPawn = It->Get() ? It->Get()->GetPawn() : nullptr;
        if (PlayerPawn)
        {
            FurthestPlayerDistance = FMath::Max(FurthestPlayerDistance, Lane->GetClosestDistanceToLocation(PlayerPawn->GetActorLocation()));
        }
    }

    return FurthestPlayerDistance;
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

    const int32 SlotIndex = VehicleIndex % VehiclesPerLane;
    const int32 LaneIndex = VehicleIndex / VehiclesPerLane;
    const float LaneStagger = (LaneIndex % 3) * (TrafficSpacing * 0.32f);
    const float RequestedDistance = GetPlayerLaneDistance(Lane) + InitialSpawnDistance + (SlotIndex * TrafficSpacing) + LaneStagger;
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

        if (Vehicle->GetLaneDistance() < GetPlayerLaneDistance(Lane) - RecycleBehindPlayerDistance)
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

    // The initial order preserves same-lane spacing: slowest is nearest the player,
    // medium is ahead of it, and fastest is furthest away.
    static const FTrafficProfile TrafficProfiles[] =
    {
        { FLinearColor(1.0f, 0.28f, 0.16f), 1400.0f, TEXT("Commuter"), FVector(4.2f, 1.9f, 1.3f), FVector(210.0f, 95.0f, 65.0f) },
        { FLinearColor(0.15f, 0.55f, 1.0f), 1750.0f, TEXT("Coupe"), FVector(3.8f, 1.75f, 1.05f), FVector(190.0f, 88.0f, 55.0f) },
        { FLinearColor(1.0f, 0.78f, 0.12f), 2100.0f, TEXT("Sport"), FVector(4.5f, 1.85f, 0.95f), FVector(225.0f, 92.0f, 52.0f) },
        { FLinearColor(0.22f, 0.72f, 0.38f), 1550.0f, TEXT("SUV"), FVector(4.6f, 2.1f, 1.55f), FVector(230.0f, 105.0f, 82.0f) },
        { FLinearColor(0.35f, 0.75f, 0.42f), 1050.0f, TEXT("Truck"), FVector(6.5f, 2.15f, 2.1f), FVector(325.0f, 108.0f, 105.0f) }
    };

    const int32 SlotIndex = VehicleIndex % VehiclesPerLane;
    const int32 LaneIndex = VehicleIndex / VehiclesPerLane;
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

    const FVector SpawnLocation = Lanes[VehicleIndex]->GetTransformAtDistance(PoolSpawnDistances[VehicleIndex]).GetLocation();
    if (const UWorld* World = GetWorld())
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            const APawn* PlayerPawn = It->Get() ? It->Get()->GetPawn() : nullptr;
            if (PlayerPawn && FVector::DistSquared2D(PlayerPawn->GetActorLocation(), SpawnLocation) < FMath::Square(MinimumPlayerSpawnDistance))
            {
                return false;
            }
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
                const float GapAlpha = FMath::GetRangePct(MinimumFollowingDistance, FollowingDistance, ClosestGap);
                const float FollowSpeed = VehicleAhead->GetCurrentSpeed() * FMath::Clamp(GapAlpha, 0.0f, 1.0f);
                SpeedLimit = FMath::Min(SpeedLimit, FollowSpeed);
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
    const int32 RequestedSlot = SlotPriority[PriorityIndex] % VehiclesPerLane;
    const int32 FirstLaneIndex = (LaneChangeAttemptCounter / UE_ARRAY_COUNT(SlotPriority)) % AvailableLanes.Num();
    ++LaneChangeAttemptCounter;

    for (int32 LaneOffset = 0; LaneOffset < AvailableLanes.Num(); ++LaneOffset)
    {
        const int32 LaneIndex = (FirstLaneIndex + LaneOffset) % AvailableLanes.Num();
        const int32 VehicleIndex = (LaneIndex * VehiclesPerLane) + RequestedSlot;
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
    if (const UWorld* World = GetWorld())
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            const APawn* PlayerPawn = It->Get() ? It->Get()->GetPawn() : nullptr;
            if (PlayerPawn && FVector::DistSquared2D(PlayerPawn->GetActorLocation(), TargetLocation) < FMath::Square(MinimumPlayerLaneChangeDistance))
            {
                return false;
            }
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

#include "OverlaneBotDriverController.h"

#include "OverlaneGameModeBase.h"
#include "OverlaneVehiclePawn.h"
#include "TrafficLanePath.h"
#include "Engine/World.h"

AOverlaneBotDriverController::AOverlaneBotDriverController()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
}

void AOverlaneBotDriverController::ConfigurePracticeRoute(ATrafficLanePath* InLanePath, float InStartingDistance)
{
    LanePath = InLanePath;
    StartingDistance = FMath::Max(0.0f, InStartingDistance);
}

void AOverlaneBotDriverController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // This controller is intentionally server-only.  The possessed pawn uses
    // the existing movement replication so clients see the same bot.
    if (!HasAuthority())
    {
        return;
    }

    AOverlaneVehiclePawn* Vehicle = Cast<AOverlaneVehiclePawn>(GetPawn());
    AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (!Vehicle || !LanePath || !GameMode || !GameMode->IsDrivingAllowed())
    {
        StopVehicleInput();
        return;
    }

    const float LaneLength = LanePath->GetLaneLength();
    if (LaneLength <= KINDA_SMALL_NUMBER)
    {
        StopVehicleInput();
        return;
    }

    const float CurrentDistance = FMath::Clamp(
        LanePath->GetClosestDistanceToLocation(Vehicle->GetActorLocation()),
        StartingDistance,
        LaneLength);
    const float TargetDistance = FMath::Min(CurrentDistance + LookAheadDistance, LaneLength);
    const FVector TargetLocation = LanePath->GetTransformAtDistance(TargetDistance).GetLocation();
    const FVector LocalTarget = Vehicle->GetActorTransform().InverseTransformPositionNoScale(TargetLocation);

    // Positive local Y means the target is to the vehicle's right in UE's
    // coordinate system, which also corresponds to positive yaw steering.
    const float HeadingError = FMath::Atan2(LocalTarget.Y, FMath::Max(LocalTarget.X, 1.0f));
    const float Steering = FMath::Clamp(HeadingError * 2.4f, -1.0f, 1.0f);

    Vehicle->SetThrottleInput(DriveThrottle);
    Vehicle->SetBrakeInput(0.0f);
    Vehicle->SetSteeringInput(Steering);
    Vehicle->SetBoostInput(false);
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

#include "TrafficLanePath.h"

#include "Components/SplineComponent.h"

ATrafficLanePath::ATrafficLanePath()
{
    LaneSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LaneSpline"));
    SetRootComponent(LaneSpline);

    ApplyDefaultStraightLayout();
}

void ATrafficLanePath::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyDefaultStraightLayout();
}

void ATrafficLanePath::BeginPlay()
{
    Super::BeginPlay();
    ApplyDefaultStraightLayout();
}

void ATrafficLanePath::ApplyDefaultStraightLayout()
{
    if (!bUseDefaultStraightLayout || !LaneSpline)
    {
        return;
    }

    LaneSpline->ClearSplinePoints(false);
    LaneSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
    LaneSpline->AddSplinePoint(FVector(DefaultLaneLength, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
    LaneSpline->UpdateSpline();
}

float ATrafficLanePath::GetLaneLength() const
{
    return LaneSpline->GetSplineLength();
}

FTransform ATrafficLanePath::GetTransformAtDistance(float Distance) const
{
    return LaneSpline->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

float ATrafficLanePath::GetClosestDistanceToLocation(const FVector& WorldLocation) const
{
    const float InputKey = LaneSpline->FindInputKeyClosestToWorldLocation(WorldLocation);
    return LaneSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

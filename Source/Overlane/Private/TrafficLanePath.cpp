#include "TrafficLanePath.h"

#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"

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

void ATrafficLanePath::CollectSortedLanes(const UObject* WorldContextObject, TArray<ATrafficLanePath*>& OutLanes)
{
    OutLanes.Reset();

    TArray<AActor*> FoundLanes;
    UGameplayStatics::GetAllActorsOfClass(WorldContextObject, ATrafficLanePath::StaticClass(), FoundLanes);

    for (AActor* LaneActor : FoundLanes)
    {
        ATrafficLanePath* Lane = Cast<ATrafficLanePath>(LaneActor);
        if (Lane && Lane->GetLaneLength() > 100.0f)
        {
            OutLanes.Add(Lane);
        }
    }

    OutLanes.Sort([](const ATrafficLanePath& A, const ATrafficLanePath& B)
    {
        return A.GetActorLocation().Y < B.GetActorLocation().Y;
    });
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

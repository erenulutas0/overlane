#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficLanePath.generated.h"

class USplineComponent;

UCLASS()
class OVERLANE_API ATrafficLanePath : public AActor
{
    GENERATED_BODY()

public:
    ATrafficLanePath();

    float GetLaneLength() const;
    FTransform GetTransformAtDistance(float Distance) const;
    float GetClosestDistanceToLocation(const FVector& WorldLocation) const;

    /**
     * Every usable lane, sorted by ascending world Y: index 0 is leftmost and
     * Last() is rightmost. Degenerate lanes are dropped so that the traffic
     * director, the game mode and the AI rival all share one index space --
     * they previously each rolled their own list with different filters.
     */
    static void CollectSortedLanes(const UObject* WorldContextObject, TArray<ATrafficLanePath*>& OutLanes);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    void ApplyDefaultStraightLayout();

    UPROPERTY(EditAnywhere, Category = "Traffic")
    bool bUseDefaultStraightLayout = true;

    UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "100.0"))
    float DefaultLaneLength = 600000.0f;

    UPROPERTY(VisibleAnywhere, Category = "Traffic")
    TObjectPtr<USplineComponent> LaneSpline;
};

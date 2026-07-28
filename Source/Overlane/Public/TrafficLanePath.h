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

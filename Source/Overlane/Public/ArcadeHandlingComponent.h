#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcadeHandlingComponent.generated.h"

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class OVERLANE_API UArcadeHandlingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UArcadeHandlingComponent();

    void SetThrottleInput(float Value);
    void SetBrakeInput(float Value);
    void SetSteeringInput(float Value);
    void SetBoostInput(bool bEnabled);
    void ResetState();
    float GetSpeedKph() const;
    float GetSpeedRatio() const;
    float GetBoostChargeRatio() const { return BoostCharge; }
    bool IsBoostActive() const { return bBoostActive; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Speed", meta = (ClampMin = "1.0"))
    float MaxForwardSpeed = 5000.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Speed", meta = (ClampMin = "1.0"))
    float MaxReverseSpeed = 1200.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Speed", meta = (ClampMin = "1.0"))
    float Acceleration = 2800.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Speed", meta = (ClampMin = "1.0"))
    float BrakingDeceleration = 7200.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Speed", meta = (ClampMin = "1.0"))
    float CoastDeceleration = 550.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Boost", meta = (ClampMin = "1.0"))
    float MaxBoostSpeed = 6800.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Boost", meta = (ClampMin = "1.0"))
    float BoostAcceleration = 4200.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Boost", meta = (ClampMin = "0.0"))
    float BoostMinimumSpeed = 850.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Boost", meta = (ClampMin = "0.0"))
    float BoostDrainPerSecond = 0.42f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Boost", meta = (ClampMin = "0.0"))
    float BoostRechargePerSecond = 0.18f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Steering", meta = (ClampMin = "1.0"))
    float SteeringRateDegreesPerSecond = 105.0f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HighSpeedSteeringMultiplier = 0.32f;

    UPROPERTY(EditAnywhere, Category = "Arcade Handling|Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CollisionSpeedMultiplier = 0.45f;

private:
    float ThrottleInput = 0.0f;
    float BrakeInput = 0.0f;
    float SteeringInput = 0.0f;
    bool bBoostRequested = false;
    bool bBoostActive = false;
    float BoostCharge = 1.0f;
    float CurrentSpeed = 0.0f;
};

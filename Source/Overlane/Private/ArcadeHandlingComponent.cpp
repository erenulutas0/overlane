#include "ArcadeHandlingComponent.h"

#include "GameFramework/Pawn.h"
#include "OverlaneGameModeBase.h"

UArcadeHandlingComponent::UArcadeHandlingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UArcadeHandlingComponent::SetThrottleInput(float Value)
{
    ThrottleInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void UArcadeHandlingComponent::SetBrakeInput(float Value)
{
    BrakeInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void UArcadeHandlingComponent::SetSteeringInput(float Value)
{
    SteeringInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UArcadeHandlingComponent::SetBoostInput(bool bEnabled)
{
    bBoostRequested = bEnabled;
}

void UArcadeHandlingComponent::SetPerformanceScale(float InScale)
{
    PerformanceScale = FMath::Clamp(InScale, 0.5f, 1.15f);
}

void UArcadeHandlingComponent::ResetState()
{
    ThrottleInput = 0.0f;
    BrakeInput = 0.0f;
    SteeringInput = 0.0f;
    bBoostRequested = false;
    bBoostActive = false;
    BoostCharge = 1.0f;
    CurrentSpeed = 0.0f;
}

float UArcadeHandlingComponent::GetSpeedKph() const
{
    return CurrentSpeed * 0.036f;
}

float UArcadeHandlingComponent::GetSpeedRatio() const
{
    return FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxBoostSpeed, 0.0f, 1.0f);
}

void UArcadeHandlingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (GameMode && !GameMode->IsDrivingAllowed())
    {
        bBoostActive = false;
        return;
    }

    APawn* VehiclePawn = Cast<APawn>(GetOwner());
    if (!VehiclePawn || (!VehiclePawn->HasAuthority() && VehiclePawn->GetNetMode() != NM_Standalone))
    {
        return;
    }

    bBoostActive = bBoostRequested
        && ThrottleInput > KINDA_SMALL_NUMBER
        && CurrentSpeed >= BoostMinimumSpeed
        && BoostCharge > KINDA_SMALL_NUMBER;

    if (bBoostActive)
    {
        BoostCharge = FMath::Max(0.0f, BoostCharge - (BoostDrainPerSecond * DeltaTime));
    }
    else
    {
        BoostCharge = FMath::Min(1.0f, BoostCharge + (BoostRechargePerSecond * DeltaTime));
    }

    // PerformanceScale is 1.0 for the human player and only moves for the AI
    // rival, where it carries both the difficulty setting and the rubber band.
    const float ActiveMaxSpeed = (bBoostActive ? MaxBoostSpeed : MaxForwardSpeed) * PerformanceScale;
    const float ActiveAcceleration = (Acceleration + (bBoostActive ? BoostAcceleration : 0.0f)) * PerformanceScale;

    if (ThrottleInput > 0.0f)
    {
        if (CurrentSpeed < ActiveMaxSpeed)
        {
            CurrentSpeed = FMath::Min(CurrentSpeed + (ThrottleInput * ActiveAcceleration * DeltaTime), ActiveMaxSpeed);
        }
        else if (!bBoostActive)
        {
            // Releasing turbo should feel like coasting back to road speed,
            // never like an abrupt 240-to-180 km/h snap.
            CurrentSpeed = FMath::FInterpTo(CurrentSpeed, MaxForwardSpeed, DeltaTime, 1.35f);
        }
    }
    else if (BrakeInput > 0.0f)
    {
        const float Deceleration = BrakeInput * BrakingDeceleration * DeltaTime;
        CurrentSpeed = CurrentSpeed > 0.0f
            ? FMath::Max(0.0f, CurrentSpeed - Deceleration)
            : FMath::Max(-MaxReverseSpeed, CurrentSpeed - Deceleration);
    }
    else
    {
        CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, CoastDeceleration);
    }

    const float SpeedRatio = GetSpeedRatio();
    const float SteeringScale = FMath::Lerp(1.0f, HighSpeedSteeringMultiplier, SpeedRatio);
    const float DirectionSign = CurrentSpeed >= 0.0f ? 1.0f : -1.0f;
    const float YawDelta = SteeringInput * SteeringRateDegreesPerSecond * SteeringScale * DirectionSign * DeltaTime;
    VehiclePawn->AddActorWorldRotation(FRotator(0.0f, YawDelta, 0.0f));

    FHitResult Hit;
    VehiclePawn->AddActorWorldOffset(VehiclePawn->GetActorForwardVector() * CurrentSpeed * DeltaTime, true, &Hit);
    if (Hit.bBlockingHit && FMath::Abs(Hit.ImpactNormal.Z) < 0.7f)
    {
        CurrentSpeed *= CollisionSpeedMultiplier;
    }
}

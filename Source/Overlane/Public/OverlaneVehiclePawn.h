#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OverlaneVehiclePawn.generated.h"

class UArcadeHandlingComponent;
class UBoxComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

UCLASS()
class OVERLANE_API AOverlaneVehiclePawn : public APawn
{
    GENERATED_BODY()

public:
    AOverlaneVehiclePawn();

    void SetThrottleInput(float Value);
    void SetBrakeInput(float Value);
    void SetSteeringInput(float Value);
    void SetBoostInput(bool bEnabled);
    float GetSpeedKph() const;
    float GetBoostChargeRatio() const;
    bool IsBoostActive() const;
    bool IsTrafficImpactFeedbackActive() const { return TrafficImpactFeedbackRemaining > 0.0f; }
    FLinearColor GetTrafficImpactFeedbackColor() const { return TrafficImpactFeedbackColor; }
    bool TryRegisterNearMiss(const FLinearColor& InColor);
    int32 GetNearMissCount() const { return NearMissCount; }
    bool IsNearMissFeedbackActive() const { return NearMissFeedbackRemaining > 0.0f; }
    FLinearColor GetNearMissFeedbackColor() const { return NearMissFeedbackColor; }
    void RegisterTrafficImpact(class ATrafficVehicleBase* TrafficVehicle);
    void StopDriving();
    void RecoverToStart();
    void SetCameraFovOffset(float InOffset);

    /**
     * Marks this pawn as an AI-driven racer rather than a human one.  The flag is
     * replicated because clients need it to label rivals; an actor tag would not
     * survive the wire.  Every scoring path is keyed off this, so a bot can share
     * the human pawn class without contributing to the human's race score.
     */
    void SetAIRacer(bool bInIsAIRacer);
    bool IsAIRacer() const { return bIsAIRacer; }

    /** Signed forward speed in cm/s, the unit the traffic and bot logic use. */
    float GetForwardSpeedCms() const;

    /** Server-side: feed one command from a remote client's move batch. */
    void EnqueueInputCommand(const struct FOverlaneInputCommand& Command);
    void ClearPendingInputCommands();

    /** Scales top speed and acceleration. Only the AI rival moves this. */
    void SetPerformanceScale(float InScale);

    /** Repaints every body slot. Used to make the AI rival visually distinct. */
    void SetBodyColor(const FLinearColor& InColor);

    /** True while the recent-impact feedback came from another racer, not traffic. */
    bool IsRivalContactFeedbackActive() const { return TrafficImpactFeedbackRemaining > 0.0f && bLastImpactWasRival; }
    void RegisterRivalImpact();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void HandleVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    UFUNCTION()
    void OnRep_OwnerServerTransform();

private:
    UPROPERTY(VisibleAnywhere, Category = "Vehicle")
    TObjectPtr<UBoxComponent> VehicleCollision;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle")
    TObjectPtr<UStaticMeshComponent> VehicleMesh;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> CabinMesh;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> FrontLightBar;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> RearLightBar;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> FrontLeftWheel;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> FrontRightWheel;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> RearLeftWheel;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle|Visual")
    TObjectPtr<UStaticMeshComponent> RearRightWheel;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> VehicleBodyMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CabinMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TireMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FrontLampMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RearLampMaterial;

    UPROPERTY(VisibleAnywhere, Category = "Vehicle")
    TObjectPtr<UArcadeHandlingComponent> ArcadeHandling;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> ChaseCamera;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
    float BaseCameraDistance = 750.0f;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
    float MaxCameraDistance = 1050.0f;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0"))
    float BaseCameraFov = 90.0f;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0"))
    float MaxCameraFov = 104.0f;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1"))
    float CameraResponseSpeed = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.0"))
    float TrafficImpactFeedbackDuration = 0.8f;

    float TrafficImpactFeedbackRemaining = 0.0f;
    FLinearColor TrafficImpactFeedbackColor = FLinearColor(1.0f, 0.25f, 0.12f);
    bool bLastImpactWasRival = false;

    UPROPERTY(EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.0"))
    float NearMissFeedbackDuration = 1.2f;

    int32 NearMissCount = 0;
    float NearMissFeedbackRemaining = 0.0f;
    FLinearColor NearMissFeedbackColor = FLinearColor::White;
    float CameraFovOffset = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.0"))
    float CollisionReleaseDistance = 800.0f;

    TSet<TWeakObjectPtr<class ATrafficVehicleBase>> ActiveTrafficCollisionContacts;

    UPROPERTY(Replicated)
    bool bIsAIRacer = false;

    UPROPERTY(Replicated)
    float ReplicatedSpeedKph = 0.0f;

    UPROPERTY(Replicated)
    float ReplicatedBoostCharge = 1.0f;

    UPROPERTY(Replicated)
    bool bReplicatedBoostActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_OwnerServerTransform)
    FTransform OwnerServerTransform;

    FTransform RecoveryTransform;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficVehicleBase.generated.h"

class ATrafficLanePath;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class OVERLANE_API ATrafficVehicleBase : public AActor
{
    GENERATED_BODY()

public:
    ATrafficVehicleBase();

    void ActivateOnLane(ATrafficLanePath* Lane, float StartDistance, float InDesiredSpeed);
    void DeactivateForPool();
    void SetTrafficColor(const FLinearColor& InColor);
    const FLinearColor& GetTrafficColor() const { return TrafficColor; }
    void MarkPlayerCollision();
    bool IsTrafficActive() const { return bTrafficActive; }
    bool IsChangingLane() const { return bChangingLane; }
    ATrafficLanePath* GetAssignedLane() const { return AssignedLane; }
    float GetLaneDistance() const { return LaneDistance; }
    float GetDesiredSpeed() const { return DesiredSpeed; }
    float GetCurrentSpeed() const { return CurrentSpeed; }
    FName GetTrafficVariantName() const { return TrafficVariantName; }
    void SetTrafficSpeedLimit(float InSpeedLimit);
    bool IsOccupyingLane(const ATrafficLanePath* Lane) const;
    bool BeginLaneChange(ATrafficLanePath* TargetLane, float DurationSeconds);
    void SetTrafficVariant(FName InVariantName, const FVector& InMeshScale, const FVector& InCollisionExtent);

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnRep_ReplicatedMovement() override;

private:
    bool MoveAlongTrafficPath(const FTransform& TargetTransform, float DistanceDelta);
    bool ShouldHoldForPlayer(const FVector& TargetLocation) const;

    /**
     * Client-side motion between snapshots.
     *
     * Without this a client's traffic sits wherever the last packet put it,
     * roughly one-way-latency behind the server -- about 175 cm at 75 ms for a
     * fast car. That error lands directly on the player's collision and near-miss
     * geometry. Extrapolating from the replicated lane speed cuts it to the
     * acceleration term only, which is centimetres.
     */
    void TickClientExtrapolation(float DeltaSeconds);

    /**
     * The server stops a traffic car dead when a player is inside a 540 x 310 uu
     * box. The near-miss window is 300 uu wide, so that freeze happens during
     * essentially every near miss -- and naive extrapolation would overshoot in
     * the dangerous direction at exactly the scoring moment. This reproduces the
     * test locally, for the local player only, which is the one case a client can
     * evaluate identically to the server.
     */
    bool ShouldHoldForLocalPlayer() const;

    UFUNCTION()
    void HandleNearMissBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleNearMissEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

    UFUNCTION()
    void OnRep_TrafficActive();

    UFUNCTION()
    void OnRep_TrafficVisualState();

    void ApplyTrafficActiveState();
    void ApplyTrafficVisualState();
    void ConfigureTrafficVisualAssetForVariant();
    void UpdateStylizedVisualGeometry();
    void UpdateTemplateSportsCarVisualGeometry();
    void UpdateTemplateOffroadVisualGeometry();

    UPROPERTY(VisibleAnywhere, Category = "Traffic")
    TObjectPtr<UBoxComponent> Collision;

    UPROPERTY(VisibleAnywhere, Category = "Traffic")
    TObjectPtr<UBoxComponent> NearMissTrigger;

    UPROPERTY(VisibleAnywhere, Category = "Traffic")
    TObjectPtr<UStaticMeshComponent> VehicleMesh;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> CabinMesh;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> FrontLightBar;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> RearLightBar;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> FrontLeftWheel;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> FrontRightWheel;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> RearLeftWheel;

    UPROPERTY(VisibleAnywhere, Category = "Traffic|Visual")
    TObjectPtr<UStaticMeshComponent> RearRightWheel;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> VehicleMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CabinMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TireMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FrontLampMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RearLampMaterial;

    // These meshes are resolved once in the actor constructor. Runtime traffic
    // variants must reuse the cached pointers: ConstructorHelpers may only be
    // called while a UObject constructor is executing.
    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> TemplateSportsCarBodyMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> TemplateSportsCarGlassMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> TemplateSportsCarWheelMesh;

    // The off-road template is intentionally a separate traffic profile.  The
    // replicated profile name below selects it on every client, rather than
    // inferring it from a local random choice or a mesh scale threshold.
    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> TemplateOffroadBodyMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> TemplateOffroadTireMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> PlaceholderBodyMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> PlaceholderWheelMesh;

    UPROPERTY(Transient)
    TObjectPtr<ATrafficLanePath> AssignedLane;

    UPROPERTY(Transient)
    TObjectPtr<ATrafficLanePath> LaneChangeTarget;

    UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
    float DesiredSpeed = 1800.0f;

    float CurrentSpeed = 0.0f;
    float TrafficSpeedLimit = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
    float MinimumNearMissSpeedKph = 90.0f;

    UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
    float MinimumRelativePassSpeedKph = 25.0f;

    UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
    float MaximumNearMissLateralDistance = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
    float MinimumNearMissForwardOffset = 220.0f;

    float LaneDistance = 0.0f;
    FLinearColor TrafficColor = FLinearColor::White;

    UPROPERTY(ReplicatedUsing = OnRep_TrafficVisualState)
    FLinearColor ReplicatedTrafficColor = FLinearColor::White;

    UPROPERTY(ReplicatedUsing = OnRep_TrafficVisualState)
    FVector ReplicatedMeshScale = FVector(4.2f, 1.9f, 1.3f);

    UPROPERTY(ReplicatedUsing = OnRep_TrafficVisualState)
    FVector ReplicatedCollisionExtent = FVector(210.0f, 95.0f, 65.0f);

    UPROPERTY(ReplicatedUsing = OnRep_TrafficVisualState)
    FName ReplicatedTrafficVariantName = TEXT("Commuter");

    /**
     * Lane speed in cm/s. Replicated rather than inferred from two position
     * samples because it already encodes both the ShouldHoldForPlayer freeze and
     * the FInterpTo acceleration, which position differencing cannot see.
     */
    UPROPERTY(Replicated)
    int16 ReplicatedLaneSpeed = 0;

    /** Seconds since the last snapshot; extrapolation stops when it runs out. */
    float ClientExtrapolationElapsed = 0.0f;

    /** Decaying visual error so a new snapshot never pops the car backwards. */
    FVector ClientSmoothingOffset = FVector::ZeroVector;

    FName TrafficVariantName = TEXT("Commuter");
    bool bUsesTemplateSportsCar = false;
    bool bUsesTemplateOffroad = false;
    bool bUsingTemplateSportsCarVisual = false;
    bool bUsingTemplateOffroadVisual = false;
    bool bNearMissEncounterActive = false;
    bool bNearMissBlocked = false;
    bool bNearMissAwarded = false;
    float LaneChangeElapsed = 0.0f;
    float LaneChangeDuration = 0.0f;
    bool bChangingLane = false;
    UPROPERTY(ReplicatedUsing = OnRep_TrafficActive)
    bool bTrafficActive = false;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OverlaneNetTypes.h"
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

    bool IsPredicting() const;
    void DrainPredictedCommands(TArray<FOverlaneInputCommand>& OutCommands);

    /** Server-side: feed one command from a remote client's move batch. */
    void EnqueueInputCommand(const struct FOverlaneInputCommand& Command);
    void ClearPendingInputCommands();

    /**
     * How far this machine's pose is from the server's last acknowledged one.
     *
     * Meaningless until prediction is enabled, and that is the point: it must
     * read zero while the client is a pure echo, which is the check that the
     * measurement itself is trustworthy before any correction is enforced.
     */
    static bool IsCorrectionDebugEnabled();
    bool HasServerGhost() const;
    FVector GetServerGhostLocation() const;

    /**
     * How far this machine's CURRENT pose is from the server's acknowledged one.
     *
     * Named "lag", not "error", because that is what it is: under prediction this
     * is the client's legitimate lead - one-way latency plus buffer depth, around
     * 8 m at 100 ms RTT and 180 km/h - and it is correct behaviour. It was
     * previously called a prediction error, which would have been read as a fault
     * and would have made any threshold derived from it useless.
     */
    float GetServerLagLongitudinalCm() const;
    float GetServerLagLateralCm() const;
    float GetServerLagYawDegrees() const;

    /** The real reconciliation error: client vs server, for the SAME sequence. */
    const FOverlaneReconcileSample& GetLastReconcileSample() const { return LastReconcileSample; }
    int32 GetAckCount() const { return AckCount; }
    int32 GetRingMissCount() const { return RingMissCount; }

    uint16 GetAckedSequence() const { return ServerMoveAck.Sequence; }

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
    void OnRep_ServerMoveAck();

    /** Wheel spin, body roll and dive. Mesh-only, never simulation state. */
    void UpdateCosmeticMotion(float DeltaSeconds, float SpeedRatio, float LongitudinalAccel);

    /**
     * Visible jolt on impact.
     *
     * Collisions previously produced a HUD string and nothing else: the car did
     * not flinch and the camera did not move, so hitting a car at 200 km/h read
     * as softer than accelerating. @param Strength is 0..1.
     */
    void TriggerImpactShake(float Strength);

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

    /**
     * Pulling the arm far back at speed shrinks the car and reduces apparent
     * road motion -- the classic toy-car-on-a-track look. FOV does that job now.
     */
    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
    float MaxCameraDistance = 820.0f;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0"))
    float BaseCameraFov = 90.0f;

    /** 104 carried heavy edge distortion; the extreme is reserved for boost. */
    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0"))
    float MaxCameraFov = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1"))
    float CameraResponseSpeed = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.0"))
    float TrafficImpactFeedbackDuration = 0.8f;

    // Cosmetic-only motion state. Deliberately NOT in FOverlaneVehicleSimState
    // and never applied to VehicleCollision: rolling the collision root would
    // change the box orientation and break the tested near-miss and traffic
    // collision behaviour. These live on the mesh and replicate to nobody.
    float WheelSpinDegrees = 0.0f;
    float CosmeticRoll = 0.0f;
    float CosmeticPitch = 0.0f;
    float PreviousForwardSpeed = 0.0f;
    float ImpactShakeRemaining = 0.0f;
    float ImpactShakeStrength = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.0"))
    float ImpactShakeDuration = 0.55f;

    /** Degrees of body lean at full steering lock and full speed. */
    UPROPERTY(EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.0"))
    float MaxCosmeticRollDegrees = 6.0f;

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

    /**
     * The server's authoritative answer to the owning client's input stream.
     *
     * Replaces a raw FTransform sent 60 times a second. Under LWC that transform
     * was ten doubles, ~80 bytes per update; this is ~15 bytes at 30 Hz and
     * carries strictly more - speed, boost charge, the collision event count and
     * the driving-allowed gate - which is what reconciliation will need.
     */
    UPROPERTY(ReplicatedUsing = OnRep_ServerMoveAck)
    FOverlaneMoveAck ServerMoveAck;

    /** Sticky ack bits, held until the client echoes the matching epoch back. */
    uint8 PendingAckFlags = 0;
    uint8 CorrectionEpoch = 0;

    void RecordReconcileSample(const FOverlaneReconcileSample& Sample);

    // --- Client-side reconciliation measurement (N-007, log only) ------------
    FOverlaneReconcileSample LastReconcileSample;
    uint16 LastMeasuredSequence = 0;
    uint8 LocalCorrectionEpoch = 0;
    int32 AckCount = 0;
    int32 DuplicateAckCount = 0;
    int32 RingMissCount = 0;
    int32 EpochResetCount = 0;

    FTransform RecoveryTransform;
};

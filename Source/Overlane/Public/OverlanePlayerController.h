#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OverlaneNetTypes.h"
#include "OverlanePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class OVERLANE_API AOverlanePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AOverlanePlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * Carries EVERY command the client has not seen acknowledged, capped at 12.
     *
     * The old design sent one unreliable RPC per input event with no resend and
     * no heartbeat, so a single dropped packet on throttle release left the
     * server holding the throttle down forever. Redundancy fixes that without
     * paying for reliable delivery, which would head-of-line block.
     */
    UFUNCTION(Server, Unreliable, WithValidation)
    void ServerSendMoveBatch(const FOverlaneMoveBatch& Batch);

    /**
     * Recovery is a teleport, so it has to be the server's decision.
     *
     * It used to run client-locally with no authority check and no cooldown, so
     * a client could reposition itself at will and the next correction would just
     * drag it back -- a rubber-band loop that also looked like a bug.
     */
    UFUNCTION(Server, Reliable)
    void ServerRequestRecovery();

private:
    void ApplyVehicleInput(float Throttle, float Brake, float Steering, bool bBoost);
    void TickLocalCommandStream(float DeltaSeconds);
    void SendMoveBatch();
    void HandleThrottle(const FInputActionValue& Value);
    void HandleBrake(const FInputActionValue& Value);
    void HandleSteering(const FInputActionValue& Value);
    void HandleBoost(const FInputActionValue& Value);
    void HandleRecovery(const FInputActionValue& Value);
    void HandlePause(const FInputActionValue& Value);
    void HandleMenuConfirm(const FInputActionValue& Value);
    void HandleOpenSettings(const FInputActionValue& Value);
    void HandleMenuUp(const FInputActionValue& Value);
    void HandleMenuDown(const FInputActionValue& Value);
    void HandleMenuAdjustLeft(const FInputActionValue& Value);
    void HandleMenuAdjustRight(const FInputActionValue& Value);
    void HandleMenuBack(const FInputActionValue& Value);
    void HandleReturnToMenu(const FInputActionValue& Value);

    float CachedThrottleInput = 0.0f;
    float CachedBrakeInput = 0.0f;
    float CachedSteeringInput = 0.0f;
    bool bCachedBoostInput = false;

    /** Commands generated but not yet known to have reached the server. */
    TArray<FOverlaneInputCommand> UnackedCommands;
    float CommandAccumulator = 0.0f;
    float SendAccumulator = 0.0f;
    uint16 NextCommandSequence = 1;

    /** Server side: the highest sequence accepted from this client so far. */
    uint16 LastAcceptedSequence = 0;

    /** Server side: world time of the last granted recovery, for the cooldown. */
    float LastRecoveryTime = -1000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Overlane|Recovery", meta = (ClampMin = "0.0"))
    float RecoveryCooldownSeconds = 5.0f;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> DrivingMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ThrottleAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> BrakeAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SteeringAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> BoostAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> RecoveryAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> PauseAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MenuConfirmAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> OpenSettingsAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MenuUpAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MenuDownAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MenuAdjustLeftAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MenuAdjustRightAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MenuBackAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ReturnToMenuAction;
};

#include "OverlanePlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "HighwayEnvironmentDirector.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneVehiclePawn.h"
#include "Kismet/GameplayStatics.h"

AOverlanePlayerController::AOverlanePlayerController()
{
    DrivingMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DrivingMappingContext"));

    ThrottleAction = CreateDefaultSubobject<UInputAction>(TEXT("ThrottleAction"));
    ThrottleAction->ValueType = EInputActionValueType::Axis1D;

    BrakeAction = CreateDefaultSubobject<UInputAction>(TEXT("BrakeAction"));
    BrakeAction->ValueType = EInputActionValueType::Axis1D;

    SteeringAction = CreateDefaultSubobject<UInputAction>(TEXT("SteeringAction"));
    SteeringAction->ValueType = EInputActionValueType::Axis1D;

    BoostAction = CreateDefaultSubobject<UInputAction>(TEXT("BoostAction"));
    BoostAction->ValueType = EInputActionValueType::Boolean;

    RecoveryAction = CreateDefaultSubobject<UInputAction>(TEXT("RecoveryAction"));
    RecoveryAction->ValueType = EInputActionValueType::Boolean;

    PauseAction = CreateDefaultSubobject<UInputAction>(TEXT("PauseAction"));
    PauseAction->ValueType = EInputActionValueType::Boolean;

    MenuConfirmAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuConfirmAction"));
    MenuConfirmAction->ValueType = EInputActionValueType::Boolean;
    OpenSettingsAction = CreateDefaultSubobject<UInputAction>(TEXT("OpenSettingsAction"));
    OpenSettingsAction->ValueType = EInputActionValueType::Boolean;
    MenuUpAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuUpAction"));
    MenuUpAction->ValueType = EInputActionValueType::Boolean;
    MenuDownAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuDownAction"));
    MenuDownAction->ValueType = EInputActionValueType::Boolean;
    MenuAdjustLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuAdjustLeftAction"));
    MenuAdjustLeftAction->ValueType = EInputActionValueType::Boolean;
    MenuAdjustRightAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuAdjustRightAction"));
    MenuAdjustRightAction->ValueType = EInputActionValueType::Boolean;
    MenuBackAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuBackAction"));
    MenuBackAction->ValueType = EInputActionValueType::Boolean;
    ReturnToMenuAction = CreateDefaultSubobject<UInputAction>(TEXT("ReturnToMenuAction"));
    ReturnToMenuAction->ValueType = EInputActionValueType::Boolean;

    DrivingMappingContext->MapKey(ThrottleAction, EKeys::W);
    DrivingMappingContext->MapKey(ThrottleAction, EKeys::Gamepad_RightTrigger);
    DrivingMappingContext->MapKey(BrakeAction, EKeys::S);
    DrivingMappingContext->MapKey(BrakeAction, EKeys::Gamepad_LeftTrigger);

    FEnhancedActionKeyMapping& LeftSteering = DrivingMappingContext->MapKey(SteeringAction, EKeys::A);
    LeftSteering.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("LeftSteeringNegate")));
    DrivingMappingContext->MapKey(SteeringAction, EKeys::D);
    DrivingMappingContext->MapKey(SteeringAction, EKeys::Gamepad_LeftX);
    DrivingMappingContext->MapKey(BoostAction, EKeys::LeftShift);
    DrivingMappingContext->MapKey(BoostAction, EKeys::Gamepad_RightShoulder);
    DrivingMappingContext->MapKey(RecoveryAction, EKeys::R);
    DrivingMappingContext->MapKey(RecoveryAction, EKeys::Gamepad_LeftThumbstick);
    DrivingMappingContext->MapKey(PauseAction, EKeys::P);
    DrivingMappingContext->MapKey(PauseAction, EKeys::Gamepad_Special_Right);
    DrivingMappingContext->MapKey(MenuConfirmAction, EKeys::Enter);
    DrivingMappingContext->MapKey(MenuConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
    DrivingMappingContext->MapKey(OpenSettingsAction, EKeys::O);
    DrivingMappingContext->MapKey(OpenSettingsAction, EKeys::Tab);
    DrivingMappingContext->MapKey(OpenSettingsAction, EKeys::F2);
    DrivingMappingContext->MapKey(OpenSettingsAction, EKeys::Gamepad_FaceButton_Top);
    DrivingMappingContext->MapKey(MenuUpAction, EKeys::Up);
    DrivingMappingContext->MapKey(MenuUpAction, EKeys::W);
    DrivingMappingContext->MapKey(MenuUpAction, EKeys::Gamepad_DPad_Up);
    DrivingMappingContext->MapKey(MenuDownAction, EKeys::Down);
    DrivingMappingContext->MapKey(MenuDownAction, EKeys::S);
    DrivingMappingContext->MapKey(MenuDownAction, EKeys::Gamepad_DPad_Down);
    DrivingMappingContext->MapKey(MenuAdjustLeftAction, EKeys::Left);
    DrivingMappingContext->MapKey(MenuAdjustLeftAction, EKeys::A);
    DrivingMappingContext->MapKey(MenuAdjustLeftAction, EKeys::Gamepad_DPad_Left);
    DrivingMappingContext->MapKey(MenuAdjustRightAction, EKeys::Right);
    DrivingMappingContext->MapKey(MenuAdjustRightAction, EKeys::D);
    DrivingMappingContext->MapKey(MenuAdjustRightAction, EKeys::Gamepad_DPad_Right);
    DrivingMappingContext->MapKey(MenuBackAction, EKeys::Escape);
    DrivingMappingContext->MapKey(MenuBackAction, EKeys::Gamepad_FaceButton_Right);
    DrivingMappingContext->MapKey(ReturnToMenuAction, EKeys::M);
    DrivingMappingContext->MapKey(ReturnToMenuAction, EKeys::Gamepad_Special_Left);
}

void AOverlanePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            InputSubsystem->AddMappingContext(DrivingMappingContext, 0);
        }
    }

    // The highway art layer is deterministic and purely local. Each PIE world
    // constructs one copy, avoiding unnecessary replication for thousands of
    // decorative instances while keeping host and client visuals identical.
    if (IsLocalController() && GetWorld() && GetWorld()->IsGameWorld())
    {
        TArray<AActor*> ExistingEnvironmentDirectors;
        UGameplayStatics::GetAllActorsOfClass(this, AHighwayEnvironmentDirector::StaticClass(), ExistingEnvironmentDirectors);
        if (ExistingEnvironmentDirectors.IsEmpty())
        {
            GetWorld()->SpawnActor<AHighwayEnvironmentDirector>();
        }
    }
}

void AOverlanePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ThisClass::HandleThrottle);
        EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ThisClass::HandleThrottle);
        EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ThisClass::HandleBrake);
        EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ThisClass::HandleBrake);
        EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ThisClass::HandleSteering);
        EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ThisClass::HandleSteering);
        EnhancedInputComponent->BindAction(BoostAction, ETriggerEvent::Triggered, this, &ThisClass::HandleBoost);
        EnhancedInputComponent->BindAction(BoostAction, ETriggerEvent::Completed, this, &ThisClass::HandleBoost);
        EnhancedInputComponent->BindAction(RecoveryAction, ETriggerEvent::Started, this, &ThisClass::HandleRecovery);
        EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &ThisClass::HandlePause);
        EnhancedInputComponent->BindAction(MenuConfirmAction, ETriggerEvent::Started, this, &ThisClass::HandleMenuConfirm);
        EnhancedInputComponent->BindAction(OpenSettingsAction, ETriggerEvent::Started, this, &ThisClass::HandleOpenSettings);
        EnhancedInputComponent->BindAction(MenuUpAction, ETriggerEvent::Started, this, &ThisClass::HandleMenuUp);
        EnhancedInputComponent->BindAction(MenuDownAction, ETriggerEvent::Started, this, &ThisClass::HandleMenuDown);
        EnhancedInputComponent->BindAction(MenuAdjustLeftAction, ETriggerEvent::Started, this, &ThisClass::HandleMenuAdjustLeft);
        EnhancedInputComponent->BindAction(MenuAdjustRightAction, ETriggerEvent::Started, this, &ThisClass::HandleMenuAdjustRight);
        EnhancedInputComponent->BindAction(MenuBackAction, ETriggerEvent::Started, this, &ThisClass::HandleMenuBack);
        EnhancedInputComponent->BindAction(ReturnToMenuAction, ETriggerEvent::Started, this, &ThisClass::HandleReturnToMenu);
    }
}

void AOverlanePlayerController::HandleThrottle(const FInputActionValue& Value)
{
    CachedThrottleInput = FMath::Clamp(Value.Get<float>(), 0.0f, 1.0f);
    SubmitVehicleInput();
}

void AOverlanePlayerController::HandleBrake(const FInputActionValue& Value)
{
    CachedBrakeInput = FMath::Clamp(Value.Get<float>(), 0.0f, 1.0f);
    SubmitVehicleInput();
}

void AOverlanePlayerController::HandleSteering(const FInputActionValue& Value)
{
    CachedSteeringInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
    SubmitVehicleInput();
}

void AOverlanePlayerController::HandleBoost(const FInputActionValue& Value)
{
    bCachedBoostInput = Value.Get<bool>();
    SubmitVehicleInput();
}

void AOverlanePlayerController::SubmitVehicleInput()
{
    if (HasAuthority())
    {
        ApplyVehicleInput(CachedThrottleInput, CachedBrakeInput, CachedSteeringInput, bCachedBoostInput);
    }
    else
    {
        ServerSetVehicleInput(CachedThrottleInput, CachedBrakeInput, CachedSteeringInput, bCachedBoostInput);
    }
}

void AOverlanePlayerController::ServerSetVehicleInput_Implementation(float Throttle, float Brake, float Steering, bool bBoost)
{
    ApplyVehicleInput(Throttle, Brake, Steering, bBoost);
}

void AOverlanePlayerController::ApplyVehicleInput(float Throttle, float Brake, float Steering, bool bBoost)
{
    if (AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetPawn()))
    {
        const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
        const bool bDrivingAllowed = GameMode && GameMode->IsDrivingAllowed();
        VehiclePawn->SetThrottleInput(bDrivingAllowed ? FMath::Clamp(Throttle, 0.0f, 1.0f) : 0.0f);
        VehiclePawn->SetBrakeInput(bDrivingAllowed ? FMath::Clamp(Brake, 0.0f, 1.0f) : 0.0f);
        VehiclePawn->SetSteeringInput(bDrivingAllowed ? FMath::Clamp(Steering, -1.0f, 1.0f) : 0.0f);
        VehiclePawn->SetBoostInput(bDrivingAllowed && bBoost);
    }
}

void AOverlanePlayerController::HandleRecovery(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsMainMenuVisible() || GameMode->IsSettingsVisible())
        {
            return;
        }

        // During a pause, R means a full race restart. During active driving it
        // remains the lightweight vehicle recovery control.
        if (GameMode->IsRaceFinished() || GameMode->IsRacePaused())
        {
            GameMode->RestartRace();
            return;
        }
    }

    if (AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetPawn()))
    {
        VehiclePawn->RecoverToStart();
    }
}

void AOverlanePlayerController::HandlePause(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsSettingsVisible())
        {
            GameMode->CloseSettings();
        }
        else if (!GameMode->IsMainMenuVisible())
        {
            GameMode->ToggleRacePause();
        }
    }
}

void AOverlanePlayerController::HandleMenuConfirm(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        GameMode->ConfirmMenuSelection();
    }
}

void AOverlanePlayerController::HandleOpenSettings(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        GameMode->OpenSettings();
    }
}

void AOverlanePlayerController::HandleMenuUp(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        GameMode->NavigateSettings(-1);
    }
}

void AOverlanePlayerController::HandleMenuDown(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        GameMode->NavigateSettings(1);
    }
}

void AOverlanePlayerController::HandleMenuAdjustLeft(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsSettingsVisible())
        {
            GameMode->AdjustSelectedSetting(-1);
        }
        else
        {
            GameMode->NavigateMenu(-1);
        }
    }
}

void AOverlanePlayerController::HandleMenuAdjustRight(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsSettingsVisible())
        {
            GameMode->AdjustSelectedSetting(1);
        }
        else
        {
            GameMode->NavigateMenu(1);
        }
    }
}

void AOverlanePlayerController::HandleMenuBack(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsSettingsVisible())
        {
            GameMode->CloseSettings();
        }
        else if (GameMode->IsRacePaused())
        {
            GameMode->ToggleRacePause();
        }
        else if (GameMode->IsRaceActive())
        {
            GameMode->ToggleRacePause();
        }
    }
}

void AOverlanePlayerController::HandleReturnToMenu(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsRacePaused() || GameMode->IsRaceFinished())
        {
            GameMode->ReturnToMainMenu();
        }
    }
}

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
    // The command stream is sampled on a fixed cadence in Tick, so ticking is no
    // longer optional: without it a client would never send input at all.
    PrimaryActorTick.bCanEverTick = true;

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
    // Escape only reaches the game in a packaged build: in PIE the editor
    // consumes it and stops the session, so a back key that also works while
    // testing in the editor is mapped alongside it.
    DrivingMappingContext->MapKey(MenuBackAction, EKeys::Escape);
    DrivingMappingContext->MapKey(MenuBackAction, EKeys::BackSpace);
    DrivingMappingContext->MapKey(MenuBackAction, EKeys::Q);
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
    // The Handle* functions only ever update the cache now. Sampling happens once
    // per fixed step in Tick, which throttles what used to be a ~432 RPC/s flood
    // from ETriggerEvent::Triggered firing every frame on every axis.
    CachedThrottleInput = FMath::Clamp(Value.Get<float>(), 0.0f, 1.0f);
}

void AOverlanePlayerController::HandleBrake(const FInputActionValue& Value)
{
    CachedBrakeInput = FMath::Clamp(Value.Get<float>(), 0.0f, 1.0f);
}

void AOverlanePlayerController::HandleSteering(const FInputActionValue& Value)
{
    CachedSteeringInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
}

void AOverlanePlayerController::HandleBoost(const FInputActionValue& Value)
{
    bCachedBoostInput = Value.Get<bool>();
}

void AOverlanePlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!IsLocalController())
    {
        return;
    }

    if (HasAuthority())
    {
        // Standalone, or the listen-server host driving its own car: the pawn's
        // handling component samples the cache directly, so there is no command
        // stream to build and nothing to send.
        ApplyVehicleInput(CachedThrottleInput, CachedBrakeInput, CachedSteeringInput, bCachedBoostInput);
        return;
    }

    TickLocalCommandStream(DeltaSeconds);
}

void AOverlanePlayerController::TickLocalCommandStream(float DeltaSeconds)
{
    AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetPawn());

    // When prediction is on, the handling component already generated and
    // simulated this frame's commands on its own accumulator. Generating a second
    // stream here would give the client two different sequence spaces for the
    // same inputs and make the ack unmatchable.
    if (VehiclePawn && VehiclePawn->IsPredicting())
    {
        VehiclePawn->DrainPredictedCommands(UnackedCommands);
    }
    else
    {
        CommandAccumulator += FMath::Min(DeltaSeconds, 0.25f);

        int32 Generated = 0;
        while (CommandAccumulator >= OverlaneFixedDeltaSeconds && Generated < OverlaneMaxStepsPerFrame)
        {
            UnackedCommands.Add(FOverlaneInputCommand::Make(
                NextCommandSequence++, CachedThrottleInput, CachedBrakeInput, CachedSteeringInput, bCachedBoostInput));

            CommandAccumulator -= OverlaneFixedDeltaSeconds;
            ++Generated;
        }
    }

    // The remainder is deliberately kept, matching the server's accumulator.
    // Zeroing it on one side and not the other is exactly the asymmetry that
    // makes two machines disagree about how many steps a second contained.

    // Until reconciliation lands there is no ack channel, so the window is simply
    // capped: the newest 12 commands are 200 ms of redundancy, which is what a
    // dropped packet needs to be recoverable.
    if (UnackedCommands.Num() > OverlaneMaxBatchCommands)
    {
        UnackedCommands.RemoveAt(0, UnackedCommands.Num() - OverlaneMaxBatchCommands, EAllowShrinking::No);
    }

    // Subtract rather than zero, or the send rate is quantised to the frame rate
    // and is never actually the 30 Hz it claims to be.
    SendAccumulator += DeltaSeconds;
    const float SendInterval = 1.0f / 30.0f;
    if (SendAccumulator >= SendInterval)
    {
        SendAccumulator = FMath::Min(SendAccumulator - SendInterval, SendInterval);
        SendMoveBatch();
    }
}

void AOverlanePlayerController::SendMoveBatch()
{
    if (UnackedCommands.Num() == 0)
    {
        return;
    }

    FOverlaneMoveBatch Batch;
    Batch.BaseSequence = UnackedCommands[0].Sequence;
    Batch.Commands = UnackedCommands;
    ServerSendMoveBatch(Batch);
}

bool AOverlanePlayerController::ServerSendMoveBatch_Validate(const FOverlaneMoveBatch& Batch)
{
    return Batch.Commands.Num() > 0 && Batch.Commands.Num() <= OverlaneMaxBatchCommands;
}

void AOverlanePlayerController::ServerSendMoveBatch_Implementation(const FOverlaneMoveBatch& Batch)
{
    AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetPawn());

    // Refill a token bucket so a client cannot buy extra simulated steps by
    // sending faster. 60 tokens per second is exactly the honest rate; the burst
    // of 4 absorbs normal packet clumping.
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float Elapsed = FMath::Max(0.0f, Now - LastBatchTime);
    LastBatchTime = Now;
    CommandTokens = FMath::Min(CommandTokens + (Elapsed / OverlaneFixedDeltaSeconds), 4.0f + (1.0f / OverlaneFixedDeltaSeconds));

    for (const FOverlaneInputCommand& Command : Batch.Commands)
    {
        // Sequence numbers wrap, so compare as a signed difference rather than
        // with <, or the stream would stall for good after 65535 commands.
        const int16 Delta = static_cast<int16>(Command.Sequence - LastAcceptedSequence);
        if (Delta <= 0)
        {
            continue;   // already have it: this is the redundancy doing its job
        }

        // Advance the accepted sequence even with no pawn. Not doing so meant the
        // pawnless window during travel or respawn made every subsequent batch
        // look implausibly far ahead and manufactured a resync on every client.
        if (!VehiclePawn)
        {
            LastAcceptedSequence = Command.Sequence;
            continue;
        }

        if (Delta > 24)
        {
            // Implausibly far ahead. Resynchronise rather than banking a burst
            // of future input a client could later cash in as a speed boost.
            VehiclePawn->ClearPendingInputCommands();
            LastAcceptedSequence = static_cast<uint16>(Command.Sequence - 1);
        }

        if (CommandTokens < 1.0f)
        {
            // Over budget: accept the sequence so the stream stays in step, but
            // do not hand the client a step it has not earned in wall time.
            LastAcceptedSequence = Command.Sequence;
            continue;
        }

        CommandTokens -= 1.0f;
        VehiclePawn->EnqueueInputCommand(Command);
        LastAcceptedSequence = Command.Sequence;
    }
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

    // A teleport is the server's call. In standalone this still lands on the
    // server implementation directly, so solo play is unchanged.
    ServerRequestRecovery();
}

void AOverlanePlayerController::ServerRequestRecovery_Implementation()
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const AOverlaneGameModeBase* GameMode = World->GetAuthGameMode<AOverlaneGameModeBase>();
    if (!GameMode || !GameMode->IsDrivingAllowed())
    {
        return;
    }

    const float Now = World->GetTimeSeconds();
    if (Now - LastRecoveryTime < RecoveryCooldownSeconds)
    {
        return;
    }

    if (AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetPawn()))
    {
        LastRecoveryTime = Now;
        VehiclePawn->RecoverToStart();
    }
}

void AOverlanePlayerController::HandlePause(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsSessionBrowserVisible())
        {
            GameMode->CloseSessionBrowser();
        }
        else if (GameMode->IsSettingsVisible())
        {
            GameMode->CloseSettings();
        }
        else if (!GameMode->IsMainMenuVisible() && !GameMode->IsOnlineLobbyVisible())
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
        // The session browser is a vertical list, so up/down walks it. Everywhere
        // else up/down still belongs to the settings rows.
        if (GameMode->IsSessionBrowserVisible())
        {
            GameMode->NavigateMenu(-1);
        }
        else
        {
            GameMode->NavigateSettings(-1);
        }
    }
}

void AOverlanePlayerController::HandleMenuDown(const FInputActionValue& Value)
{
    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        if (GameMode->IsSessionBrowserVisible())
        {
            GameMode->NavigateMenu(1);
        }
        else
        {
            GameMode->NavigateSettings(1);
        }
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
        if (GameMode->IsSessionBrowserVisible())
        {
            GameMode->CloseSessionBrowser();
        }
        else if (GameMode->IsSettingsVisible())
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

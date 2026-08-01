#include "OverlaneGameModeBase.h"

#include "OverlaneBotDriverController.h"
#include "OverlaneHUD.h"
#include "OverlanePlayerController.h"
#include "OverlaneProgressSaveGame.h"
#include "OverlaneRaceGameState.h"
#include "OverlaneSessionSubsystem.h"
#include "OverlaneSettingsSaveGame.h"
#include "OverlaneVehiclePawn.h"
#include "TrafficDirector.h"
#include "TrafficLanePath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#include "GameFramework/PlayerState.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    /**
     * Free roam: keep the traffic and the route, drop the race around them.
     *
     * The map is about to be rebuilt with curves and gradients, and inspecting it
     * while a countdown, a finish line and a rival are all running gets in the way -
     * you cannot stop and look at a bridge if the race ends 6 km later. It also
     * sidesteps a real problem for the duration: the curved-road analysis found that
     * curvature breaks the rival's merge-completion test outright (pure-pursuit
     * cross-track lag reaches 144 cm against a LaneCompleteTolerance of 120), so with
     * no rival on the road that defect cannot confuse map work.
     *
     * Deliberately a cvar rather than a menu entry. This is a development tool for
     * looking at the level, not a game mode, and it should not need UI work to exist.
     */
    static TAutoConsoleVariable<int32> CVarFreeRoam(
        TEXT("overlane.FreeRoam"),
        0,
        TEXT("1 = no countdown, no finish, no rival. Traffic and the route still run, for inspecting the map."),
        ECVF_Default);

    bool IsFreeRoamEnabled()
    {
        if (CVarFreeRoam.GetValueOnAnyThread() != 0)
        {
            return true;
        }

        // Fall back to reading the ini directly, because the cvar route silently
        // loses the setting. [SystemSettings] is applied while engine config loads,
        // but a TAutoConsoleVariable declared in a game module only registers when
        // that module loads - which is later. The assignment therefore targets a
        // variable that does not exist yet and is dropped without a warning, which is
        // exactly why the race kept running after the setting was added.
        static int32 ConfigValue = -1;
        if (ConfigValue < 0)
        {
            bool bFromConfig = false;
            ConfigValue = (GConfig && GConfig->GetBool(TEXT("SystemSettings"), TEXT("overlane.FreeRoam"), bFromConfig, GEngineIni) && bFromConfig)
                ? 1
                : 0;
        }
        return ConfigValue != 0;
    }
}

AOverlaneGameModeBase::AOverlaneGameModeBase()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AOverlaneVehiclePawn::StaticClass();
    PlayerControllerClass = AOverlanePlayerController::StaticClass();
    HUDClass = AOverlaneHUD::StaticClass();
    GameStateClass = AOverlaneRaceGameState::StaticClass();
}

void AOverlaneGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (World && World->IsGameWorld())
    {
        LocalSettings = Cast<UOverlaneSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("OverlaneLocalSettings"), 0));
        if (!LocalSettings)
        {
            LocalSettings = Cast<UOverlaneSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UOverlaneSettingsSaveGame::StaticClass()));
        }

        // GetSettingsLine indexes fixed name arrays with these values, so a save
        // written by an older or edited build must not be trusted unclamped.
        LocalSettings->GraphicsQuality = FMath::Clamp(LocalSettings->GraphicsQuality, 0, 3);
        LocalSettings->FrameRateMode = FMath::Clamp(LocalSettings->FrameRateMode, 0, 2);
        LocalSettings->RivalDifficulty = FMath::Clamp(LocalSettings->RivalDifficulty, 0, 2);

        LocalProgress = Cast<UOverlaneProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("OverlaneSoloProgress"), 0));
        if (!LocalProgress)
        {
            LocalProgress = Cast<UOverlaneProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UOverlaneProgressSaveGame::StaticClass()));
        }

        if (UGameplayStatics::HasOption(OptionsString, TEXT("AutoStart")))
        {
            StartSoloRace();
        }
        else if (World->GetNetMode() == NM_ListenServer)
        {
            // Arriving here means the session subsystem just travelled us onto the
            // map as a host. Sit in a lobby waiting for friends instead of dropping
            // the host back onto the solo main menu.
            bShowingMainMenu = false;
            bInOnlineLobby = true;
        }

        SyncNetworkRaceState();
    }
}

void AOverlaneGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!NewPlayer || GetNumPlayers() <= 1)
    {
        return;
    }

    bMultiplayerRace = true;

    // The practice bot is a standalone-only construct: its controller does not
    // replicate and its pawn has no owning connection, so leaving it alive once a
    // client joins would put an invisible, unsynchronised car on the road.
    DestroyPracticeBot();

    // If a client joins an already-running solo test, replace the local pool
    // with the small server-authoritative multiplayer pool.
    if (LocalTrafficDirector)
    {
        LocalTrafficDirector->Destroy();
        LocalTrafficDirector = nullptr;

        if (bRaceActive && GetWorld())
        {
            LocalTrafficDirector = GetWorld()->SpawnActor<ATrafficDirector>();
        }
    }

    SyncNetworkRaceState();
}

void AOverlaneGameModeBase::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);

    if (!NewPlayer || GetNumPlayers() <= 1)
    {
        return;
    }

    // The first network pass uses the three existing traffic lanes as clean
    // spawn lanes. The host stays centered and joining players alternate left/right.
    const int32 PlayerIndex = GetNumPlayers() - 1;
    const float SpawnLaneY = PlayerIndex % 2 == 1 ? 600.0f : -600.0f;
    if (APawn* PlayerPawn = NewPlayer->GetPawn())
    {
        FVector SpawnLocation = PlayerPawn->GetActorLocation();
        SpawnLocation.Y = SpawnLaneY;
        PlayerPawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void AOverlaneGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bSettingsAppliedToPawn)
    {
        if (AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            PlayerVehicle->SetCameraFovOffset(LocalSettings ? LocalSettings->CameraFovOffset : 0.0f);
            bSettingsAppliedToPawn = true;
        }
    }

    if (!bShowingMainMenu && !bShowingSettings && !bRaceActive && !bRaceFinished && GetWorld() && GetWorld()->GetTimeSeconds() >= RaceStartTime)
    {
        bRaceActive = true;
    }

    SyncNetworkRaceState();

    if (!bRaceActive || bRaceFinished || !GetWorld())
    {
        return;
    }

    if (bRacePaused)
    {
        return;
    }

    if (AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const int32 RouteProgressMeters = FMath::FloorToInt(FMath::Max(0.0f, PlayerVehicle->GetActorLocation().X - RouteStartX) / 100.0f);
        RaceScore = FMath::Max(0, RouteProgressMeters + NearMissScore - (CollisionCount * CollisionPenaltyPoints));
        MaxSpeedKph = FMath::Max(MaxSpeedKph, PlayerVehicle->GetSpeedKph());
        CurrentCleanDriveSeconds += DeltaSeconds;
        LongestCleanDriveSeconds = FMath::Max(LongestCleanDriveSeconds, CurrentCleanDriveSeconds);

        APlayerController* WinningPlayer = nullptr;
        for (FConstPlayerControllerIterator PlayerControllerIt = GetWorld()->GetPlayerControllerIterator(); PlayerControllerIt; ++PlayerControllerIt)
        {
            APlayerController* PlayerController = PlayerControllerIt->Get();
            const APawn* RacePawn = PlayerController ? PlayerController->GetPawn() : nullptr;
            // Free roam has no finish: the route is there to be looked at.
            if (!IsFreeRoamEnabled() && RacePawn && RacePawn->GetActorLocation().X >= RouteFinishX)
            {
                WinningPlayer = PlayerController;
                break;
            }
        }

        if (WinningPlayer)
        {
            FinishRace(WinningPlayer);
            return;
        }
    }

    // The standalone practice bot is not a PlayerController, so it is
    // intentionally checked after human racers: a same-frame tie favours the
    // human. This sits outside the player-pawn block on purpose -- nesting it
    // there meant a momentarily missing human pawn made the bot unable to ever
    // finish, which stalls the race forever.
    if (PracticeBotController)
    {
        const APawn* BotPawn = PracticeBotController->GetPawn();
        if (BotPawn && BotPawn->GetActorLocation().X >= RouteFinishX)
        {
            FinishRace(nullptr, true);
        }
    }
}

FString AOverlaneGameModeBase::GetRaceCountdownText() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return FString();
    }

    if (bShowingMainMenu || bShowingSettings)
    {
        return FString();
    }

    const float CurrentTime = World->GetTimeSeconds();
    if (CurrentTime < RaceStartTime)
    {
        return FString::FromInt(FMath::CeilToInt(RaceStartTime - CurrentTime));
    }

    return CurrentTime < RaceStartTime + GoMessageDuration ? TEXT("GO!") : FString();
}

int32 AOverlaneGameModeBase::GetRemainingDistanceMeters() const
{
    if (bRaceFinished)
    {
        return 0;
    }

    const APawn* PlayerPawn = GetWorld() ? UGameplayStatics::GetPlayerPawn(this, 0) : nullptr;
    const float PlayerX = PlayerPawn ? PlayerPawn->GetActorLocation().X : RouteStartX;
    return FMath::CeilToInt(FMath::Max(0.0f, RouteFinishX - PlayerX) / 100.0f);
}

float AOverlaneGameModeBase::GetRaceElapsedSeconds() const
{
    const UWorld* World = GetWorld();
    if (!World || RaceStartTime <= 0.0f)
    {
        return 0.0f;
    }

    const float EndTime = bRaceFinished ? RaceFinishTime : World->GetTimeSeconds();
    const float CurrentPauseDuration = bRacePaused ? EndTime - PauseStartTime : 0.0f;
    return FMath::Max(0.0f, EndTime - RaceStartTime - AccumulatedPausedTime - CurrentPauseDuration);
}

FString AOverlaneGameModeBase::GetRaceResultText() const
{
    return bRaceFinished ? FString::Printf(TEXT("BITIS! %.1f SN"), GetRaceElapsedSeconds()) : FString();
}

FString AOverlaneGameModeBase::GetRaceWinnerText() const
{
    if (!bRaceFinished)
    {
        return FString();
    }

    return bPracticeBotWon ? TEXT("ANTRENMAN BOTU KAZANDI") : TEXT("SEN KAZANDIN!");
}

float AOverlaneGameModeBase::GetBestSoloTimeSeconds() const
{
    return LocalProgress ? LocalProgress->BestSoloTimeSeconds : 0.0f;
}

int32 AOverlaneGameModeBase::GetBestSoloScore() const
{
    return LocalProgress ? LocalProgress->BestSoloScore : 0;
}

bool AOverlaneGameModeBase::IsTrafficDebugOverlayVisible() const
{
    return LocalSettings && LocalSettings->bTrafficDebugOverlay;
}

int32 AOverlaneGameModeBase::GetActiveTrafficCount() const
{
    return LocalTrafficDirector ? LocalTrafficDirector->GetActiveVehicleCount() : 0;
}

int32 AOverlaneGameModeBase::GetTrafficPoolSize() const
{
    return LocalTrafficDirector ? LocalTrafficDirector->GetPoolSize() : 0;
}

FString AOverlaneGameModeBase::GetSettingsLine(int32 Index) const
{
    if (!LocalSettings)
    {
        return FString();
    }

    static const TCHAR* QualityNames[] = { TEXT("DUSUK"), TEXT("ORTA"), TEXT("YUKSEK"), TEXT("EPIC") };
    static const TCHAR* FrameRateNames[] = { TEXT("60 FPS"), TEXT("120 FPS"), TEXT("SINIRSIZ") };
    static const TCHAR* DifficultyNames[] = { TEXT("KOLAY"), TEXT("NORMAL"), TEXT("ZOR") };
    switch (Index)
    {
    case 0: return FString::Printf(TEXT("GORUNTU KALITESI: %s"), QualityNames[LocalSettings->GraphicsQuality]);
    case 1: return FString::Printf(TEXT("VSYNC: %s"), LocalSettings->bVSyncEnabled ? TEXT("ACIK") : TEXT("KAPALI"));
    case 2: return FString::Printf(TEXT("KARE SINIRI: %s"), FrameRateNames[LocalSettings->FrameRateMode]);
    case 3: return FString::Printf(TEXT("KAMERA FOV: %+.0f"), LocalSettings->CameraFovOffset);
    case 4: return FString::Printf(TEXT("TRAFIK DEBUG: %s"), LocalSettings->bTrafficDebugOverlay ? TEXT("ACIK") : TEXT("KAPALI"));
    case 5: return FString::Printf(TEXT("RAKIP ZORLUGU: %s"), DifficultyNames[FMath::Clamp(LocalSettings->RivalDifficulty, 0, 2)]);
    default: return FString();
    }
}

void AOverlaneGameModeBase::StartSoloRace()
{
    UWorld* World = GetWorld();
    if (!World || bRaceActive || bRaceFinished)
    {
        return;
    }

    bShowingMainMenu = false;
    bShowingSettings = false;
    bSettingsOpenedFromPause = false;
    MenuSelection = 0;
    bRacePaused = false;
    AccumulatedPausedTime = 0.0f;
    PauseStartTime = 0.0f;
    MaxSpeedKph = 0.0f;
    CurrentCleanDriveSeconds = 0.0f;
    LongestCleanDriveSeconds = 0.0f;
    LastCollisionTime = -1000.0f;
    CollisionCount = 0;
    NearMissScore = 0;
    RaceScore = 0;
    bNewPersonalBest = false;
    bPracticeBotWon = false;
    WinningPlayerId = INDEX_NONE;
    // Free roam skips the countdown: there is nothing to count down to.
    RaceStartTime = World->GetTimeSeconds() + (IsFreeRoamEnabled() ? 0.0f : CountdownDuration);
    if (!LocalTrafficDirector)
    {
        LocalTrafficDirector = World->SpawnActor<ATrafficDirector>();
    }

    if (!IsFreeRoamEnabled())
    {
        SpawnPracticeBotForSoloRace();
    }

    SyncNetworkRaceState();
}

void AOverlaneGameModeBase::SpawnPracticeBotForSoloRace()
{
    UWorld* World = GetWorld();
    if (!World || !bEnablePracticeBotInStandalone || PracticeBotController || World->GetNetMode() != NM_Standalone)
    {
        return;
    }

    AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!PlayerVehicle)
    {
        return;
    }

    // One shared, length-filtered, Y-sorted lane list, so the bot, the traffic
    // director and this spawn all agree on what lane index N means.
    TArray<ATrafficLanePath*> SortedLanes;
    ATrafficLanePath::CollectSortedLanes(this, SortedLanes);

    // The player starts in the middle lane, so the rival starts in the outer
    // right one. It is free to leave that lane as soon as it is held up.
    ATrafficLanePath* BotLane = SortedLanes.Num() > 0 ? SortedLanes.Last() : nullptr;
    if (!BotLane)
    {
        return;
    }

    const float PlayerLaneDistance = BotLane->GetClosestDistanceToLocation(PlayerVehicle->GetActorLocation());
    const float BotDistance = FMath::Clamp(
        PlayerLaneDistance + PracticeBotStartAheadDistance,
        0.0f,
        FMath::Max(0.0f, BotLane->GetLaneLength() - 500.0f));

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AOverlaneVehiclePawn* BotVehicle = World->SpawnActor<AOverlaneVehiclePawn>(
        AOverlaneVehiclePawn::StaticClass(), BotLane->GetTransformAtDistance(BotDistance), SpawnParameters);
    if (!BotVehicle)
    {
        return;
    }

    // A replicated flag, not an actor tag: tags never reach clients, and every
    // scoring path needs to be able to tell a bot pawn from a human one.
    BotVehicle->SetAIRacer(true);

    PracticeBotController = World->SpawnActor<AOverlaneBotDriverController>(
        AOverlaneBotDriverController::StaticClass(), BotVehicle->GetActorLocation(), BotVehicle->GetActorRotation(), SpawnParameters);
    if (!PracticeBotController)
    {
        BotVehicle->Destroy();
        return;
    }

    PracticeBotController->ConfigurePracticeRoute(BotLane, BotDistance);
    PracticeBotController->ConfigureDifficulty(LocalSettings ? LocalSettings->RivalDifficulty : 1);
    PracticeBotController->Possess(BotVehicle);
}

FString AOverlaneGameModeBase::GetRivalDebugText() const
{
    if (!PracticeBotController)
    {
        return FString();
    }

    const float BlockerGap = PracticeBotController->GetTrackedGapMeters();
    static const TCHAR* DifficultyNames[] = { TEXT("KOLAY"), TEXT("NORMAL"), TEXT("ZOR") };
    return FString::Printf(
        TEXT("BOT: %d KM/H  ONDEKI: %s  SERIT %d  MERGE %d/%d  RED kazanc:%d guvenlik:%d  TURBO:%%%d  ZORLUK:%s"),
        FMath::RoundToInt(PracticeBotController->GetBotSpeedKph()),
        BlockerGap < 0.0f ? TEXT("YOK") : *FString::Printf(TEXT("%d M"), FMath::RoundToInt(BlockerGap)),
        PracticeBotController->GetCurrentLaneIndex(),
        PracticeBotController->GetMergesCompleted(),
        PracticeBotController->GetMergesStarted(),
        PracticeBotController->GetRejectedByGain(),
        PracticeBotController->GetRejectedBySafety(),
        // The share of the race boost was actually HELD, not whether there is charge
        // to start. The old TEXT("HAZIR") readout came from the charge latch and sat
        // there for whole races in which the rival never boosted at all.
        FMath::RoundToInt(PracticeBotController->GetBoostDutyCycle() * 100.0f),
        DifficultyNames[FMath::Clamp(PracticeBotController->GetAppliedDifficulty(), 0, 2)]);
}

int32 AOverlaneGameModeBase::GetRivalGapMeters() const
{
    const APawn* BotPawn = PracticeBotController ? PracticeBotController->GetPawn() : nullptr;
    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!BotPawn || !PlayerPawn)
    {
        return 0;
    }

    // World X is the axis both finish tests use, so a lane-spline gap could
    // disagree with who actually crosses the line first.
    return FMath::RoundToInt((PlayerPawn->GetActorLocation().X - BotPawn->GetActorLocation().X) / 100.0f);
}

void AOverlaneGameModeBase::DestroyPracticeBot()
{
    if (!PracticeBotController)
    {
        return;
    }

    if (APawn* BotPawn = PracticeBotController->GetPawn())
    {
        PracticeBotController->UnPossess();
        BotPawn->Destroy();
    }

    PracticeBotController->Destroy();
    PracticeBotController = nullptr;
}

void AOverlaneGameModeBase::FinishRace(APlayerController* WinningPlayer, bool bWonByPracticeBot)
{
    UWorld* World = GetWorld();
    if (!World || bRaceFinished)
    {
        return;
    }

    bRaceFinished = true;
    bRaceActive = false;
    RaceFinishTime = World->GetTimeSeconds();
    WinningPlayerId = WinningPlayer && WinningPlayer->PlayerState ? WinningPlayer->PlayerState->GetPlayerId() : INDEX_NONE;
    bPracticeBotWon = bWonByPracticeBot;

    for (FConstPlayerControllerIterator PlayerControllerIt = World->GetPlayerControllerIterator(); PlayerControllerIt; ++PlayerControllerIt)
    {
        if (AOverlaneVehiclePawn* RaceVehicle = PlayerControllerIt->Get() ? Cast<AOverlaneVehiclePawn>(PlayerControllerIt->Get()->GetPawn()) : nullptr)
        {
            RaceVehicle->StopDriving();
        }
    }

    if (AOverlaneVehiclePawn* BotVehicle = PracticeBotController ? Cast<AOverlaneVehiclePawn>(PracticeBotController->GetPawn()) : nullptr)
    {
        BotVehicle->StopDriving();
    }

    // Personal records belong to solo mode only. A multiplayer result must not
    // overwrite the local solo leaderboard.
    if (LocalProgress && GetNumPlayers() <= 1 && !bPracticeBotWon)
    {
        const float FinishSeconds = GetRaceElapsedSeconds();
        bNewPersonalBest = LocalProgress->BestSoloTimeSeconds <= 0.0f || FinishSeconds < LocalProgress->BestSoloTimeSeconds;
        if (bNewPersonalBest)
        {
            LocalProgress->BestSoloTimeSeconds = FinishSeconds;
        }

        if (AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            LocalProgress->BestNearMissCount = FMath::Max(LocalProgress->BestNearMissCount, PlayerVehicle->GetNearMissCount());
        }
        LocalProgress->BestSoloScore = FMath::Max(LocalProgress->BestSoloScore, RaceScore);
        ++LocalProgress->CompletedSoloRaces;
        UGameplayStatics::SaveGameToSlot(LocalProgress, TEXT("OverlaneSoloProgress"), 0);
    }

    SyncNetworkRaceState();
}

void AOverlaneGameModeBase::SyncNetworkRaceState()
{
    if (AOverlaneRaceGameState* RaceGameState = GetGameState<AOverlaneRaceGameState>())
    {
        RaceGameState->SetRaceFlow(bRaceActive, bRaceFinished, bRacePaused, RaceStartTime, RaceFinishTime, RouteStartX, RouteFinishX, WinningPlayerId);
    }
}

void AOverlaneGameModeBase::OpenSettings()
{
    // The browser owns the screen while it is open; settings would draw over it.
    if (bShowingSessionBrowser)
    {
        return;
    }

    if (bShowingMainMenu || bRacePaused)
    {
        bSettingsOpenedFromPause = bRacePaused;
        bShowingSettings = true;
        SettingsSelection = 0;
    }
}

void AOverlaneGameModeBase::CloseSettings()
{
    bShowingSettings = false;
    bSettingsOpenedFromPause = false;
}

void AOverlaneGameModeBase::NavigateSettings(int32 Direction)
{
    if (bShowingSettings)
    {
        SettingsSelection = (SettingsSelection + Direction + SettingsRowCount) % SettingsRowCount;
    }
}

void AOverlaneGameModeBase::AdjustSelectedSetting(int32 Direction)
{
    if (!bShowingSettings || !LocalSettings || Direction == 0)
    {
        return;
    }

    UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    switch (SettingsSelection)
    {
    case 0:
        LocalSettings->GraphicsQuality = FMath::Clamp(LocalSettings->GraphicsQuality + Direction, 0, 3);
        if (UserSettings) UserSettings->SetOverallScalabilityLevel(LocalSettings->GraphicsQuality);
        break;
    case 1:
        LocalSettings->bVSyncEnabled = !LocalSettings->bVSyncEnabled;
        if (UserSettings) UserSettings->SetVSyncEnabled(LocalSettings->bVSyncEnabled);
        break;
    case 2:
        LocalSettings->FrameRateMode = (LocalSettings->FrameRateMode + Direction + 3) % 3;
        if (UserSettings) UserSettings->SetFrameRateLimit(LocalSettings->FrameRateMode == 0 ? 60.0f : LocalSettings->FrameRateMode == 1 ? 120.0f : 0.0f);
        break;
    case 3:
        LocalSettings->CameraFovOffset = FMath::Clamp(LocalSettings->CameraFovOffset + (2.0f * Direction), -10.0f, 10.0f);
        if (AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0))) PlayerVehicle->SetCameraFovOffset(LocalSettings->CameraFovOffset);
        break;
    case 4:
        LocalSettings->bTrafficDebugOverlay = !LocalSettings->bTrafficDebugOverlay;
        break;
    case 5:
        // Takes effect on the next race: the rival is configured once at spawn.
        LocalSettings->RivalDifficulty = FMath::Clamp(LocalSettings->RivalDifficulty + Direction, 0, 2);
        break;
    default:
        return;
    }

    if (UserSettings)
    {
        UserSettings->ApplySettings(false);
    }
    UGameplayStatics::SaveGameToSlot(LocalSettings, TEXT("OverlaneLocalSettings"), 0);
}

UOverlaneSessionSubsystem* AOverlaneGameModeBase::GetSessionSubsystem() const
{
    const UWorld* const World = GetWorld();
    UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
    return GameInstance ? GameInstance->GetSubsystem<UOverlaneSessionSubsystem>() : nullptr;
}

void AOverlaneGameModeBase::HostOnlineRace()
{
    if (UOverlaneSessionSubsystem* const Sessions = GetSessionSubsystem())
    {
        // The subsystem travels this machine onto the map as a listen server once
        // the session is created, so there is nothing else to do here.
        Sessions->HostSession(4);
    }
}

void AOverlaneGameModeBase::OpenSessionBrowser()
{
    bShowingSessionBrowser = true;
    bShowingSettings = false;
    SessionSelection = 0;
    RefreshSessionSearch();
}

void AOverlaneGameModeBase::CloseSessionBrowser()
{
    bShowingSessionBrowser = false;
    SessionSelection = 0;
}

void AOverlaneGameModeBase::RefreshSessionSearch()
{
    if (UOverlaneSessionSubsystem* const Sessions = GetSessionSubsystem())
    {
        Sessions->FindSessions();
    }
}

void AOverlaneGameModeBase::StartRaceFromLobby()
{
    if (!bInOnlineLobby)
    {
        return;
    }

    bInOnlineLobby = false;
    StartSoloRace();
}

int32 AOverlaneGameModeBase::GetFoundSessionCount() const
{
    const UOverlaneSessionSubsystem* const Sessions = GetSessionSubsystem();
    return Sessions ? Sessions->GetFoundSessionCount() : 0;
}

FString AOverlaneGameModeBase::GetFoundSessionLabel(int32 Index) const
{
    const UOverlaneSessionSubsystem* const Sessions = GetSessionSubsystem();
    return Sessions ? Sessions->GetFoundSessionLabel(Index) : FString();
}

int32 AOverlaneGameModeBase::GetConnectedPlayerCount() const
{
    // AGameModeBase::GetNumPlayers is non-const, and the HUD reads this through a
    // const game mode pointer, so count the replicated player array instead.
    const AGameStateBase* const CurrentGameState = GetGameState<AGameStateBase>();
    return CurrentGameState ? CurrentGameState->PlayerArray.Num() : 0;
}

FString AOverlaneGameModeBase::GetSessionStatusText() const
{
    const UOverlaneSessionSubsystem* const Sessions = GetSessionSubsystem();
    return Sessions ? Sessions->GetStatusText() : FString(TEXT("CEVRIMICI SERVIS YOK"));
}

void AOverlaneGameModeBase::NavigateMenu(int32 Direction)
{
    if (bShowingSettings || Direction == 0)
    {
        return;
    }

    // The session browser is its own list: navigation walks search results, and
    // an extra trailing row is reserved for "search again".
    if (bShowingSessionBrowser)
    {
        const int32 RowCount = GetFoundSessionCount() + 1;
        SessionSelection = (SessionSelection + Direction + RowCount) % RowCount;
        return;
    }

    const int32 OptionCount = bShowingMainMenu ? MainMenuOptionCount : (bRacePaused ? 4 : 0);
    if (OptionCount > 0)
    {
        MenuSelection = (MenuSelection + Direction + OptionCount) % OptionCount;
    }
}

void AOverlaneGameModeBase::ConfirmMenuSelection()
{
    if (bShowingSettings)
    {
        return;
    }

    // The browser's last row is "search again"; every earlier row is a joinable
    // host. Joining hands off to the subsystem, which client-travels this machine.
    if (bShowingSessionBrowser)
    {
        const int32 ResultCount = GetFoundSessionCount();
        if (SessionSelection >= ResultCount)
        {
            RefreshSessionSearch();
        }
        else if (UOverlaneSessionSubsystem* const Sessions = GetSessionSubsystem())
        {
            Sessions->JoinFoundSession(SessionSelection);
        }
        return;
    }

    // The host sits in the lobby until it decides everyone has arrived.
    if (bInOnlineLobby)
    {
        StartRaceFromLobby();
        return;
    }

    if (bShowingMainMenu)
    {
        switch (MenuSelection)
        {
        case 0:
            StartSoloRace();
            break;
        case 1:
            HostOnlineRace();
            break;
        case 2:
            OpenSessionBrowser();
            break;
        default:
            OpenSettings();
            break;
        }
        return;
    }

    if (!bRacePaused)
    {
        return;
    }

    switch (MenuSelection)
    {
    case 0:
        ToggleRacePause();
        break;
    case 1:
        RestartRace();
        break;
    case 2:
        ReturnToMainMenu();
        break;
    case 3:
        OpenSettings();
        break;
    default:
        break;
    }
}

void AOverlaneGameModeBase::RegisterTrafficCollision()
{
    UWorld* World = GetWorld();
    if (!World || !bRaceActive || bRacePaused)
    {
        return;
    }

    LastCollisionTime = World->GetTimeSeconds();
    ++CollisionCount;
    CurrentCleanDriveSeconds = 0.0f;
}

void AOverlaneGameModeBase::RegisterNearMiss()
{
    if (!bRaceActive || bRacePaused || bRaceFinished)
    {
        return;
    }

    NearMissScore += NearMissBonusPoints;
}

void AOverlaneGameModeBase::ToggleRacePause()
{
    UWorld* World = GetWorld();
    if (!World || !bRaceActive || bRaceFinished)
    {
        return;
    }

    if (bRacePaused)
    {
        AccumulatedPausedTime += World->GetTimeSeconds() - PauseStartTime;
        PauseStartTime = 0.0f;
        bRacePaused = false;
        return;
    }

    PauseStartTime = World->GetTimeSeconds();
    bRacePaused = true;
    MenuSelection = 0;
}

void AOverlaneGameModeBase::RestartRace()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // OpenLevel is correct for a standalone race, but a listen server needs a
    // server travel so its connected PIE/client players follow the host instead
    // of being left behind in a standalone copy of the map.
    if (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer)
    {
        const TCHAR* TravelUrl = World->GetNetMode() == NM_ListenServer
            ? TEXT("/Game/Maps/L_VehicleHandlingTest?listen?AutoStart")
            : TEXT("/Game/Maps/L_VehicleHandlingTest?AutoStart");
        World->ServerTravel(TravelUrl, false);
        return;
    }

    UGameplayStatics::OpenLevel(this, FName(TEXT("L_VehicleHandlingTest")), true, TEXT("AutoStart"));
}

void AOverlaneGameModeBase::ReturnToMainMenu()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("L_VehicleHandlingTest")));
}

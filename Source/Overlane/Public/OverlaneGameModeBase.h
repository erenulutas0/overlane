#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OverlaneGameModeBase.generated.h"

UCLASS()
class OVERLANE_API AOverlaneGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AOverlaneGameModeBase();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

    bool IsRaceActive() const { return bRaceActive; }
    bool IsRaceFinished() const { return bRaceFinished; }
    bool IsRacePaused() const { return bRacePaused; }
    bool IsMultiplayerRace() const { return bMultiplayerRace; }
    bool DidPracticeBotWin() const { return bPracticeBotWon; }
    bool IsDrivingAllowed() const { return bRaceActive && !bRacePaused; }
    bool IsSimulationActive() const { return bRaceActive && !bRacePaused; }
    FString GetRaceCountdownText() const;
    int32 GetRemainingDistanceMeters() const;
    float GetRaceElapsedSeconds() const;
    FString GetRaceResultText() const;
    FString GetRaceWinnerText() const;
    int32 GetCollisionCount() const { return CollisionCount; }
    int32 GetRaceScore() const { return RaceScore; }
    int32 GetNearMissScore() const { return NearMissScore; }
    int32 GetCollisionPenalty() const { return CollisionCount * CollisionPenaltyPoints; }
    int32 GetMaxSpeedKph() const { return FMath::RoundToInt(MaxSpeedKph); }
    float GetLongestCleanDriveSeconds() const { return LongestCleanDriveSeconds; }
    bool IsNewPersonalBest() const { return bNewPersonalBest; }
    float GetBestSoloTimeSeconds() const;
    int32 GetBestSoloScore() const;
    bool IsMainMenuVisible() const { return bShowingMainMenu; }
    bool IsSettingsVisible() const { return bShowingSettings; }

    // --- Online session flow (P5-002) ---------------------------------------
    // The host owns all of this: a joined client has no GameMode, so its HUD
    // reads the replicated lobby state from AOverlaneRaceGameState instead.
    bool IsSessionBrowserVisible() const { return bShowingSessionBrowser; }
    bool IsOnlineLobbyVisible() const { return bInOnlineLobby; }
    int32 GetSessionSelection() const { return SessionSelection; }
    int32 GetFoundSessionCount() const;
    FString GetFoundSessionLabel(int32 Index) const;
    FString GetSessionStatusText() const;
    int32 GetConnectedPlayerCount() const;
    void HostOnlineRace();
    void OpenSessionBrowser();
    void CloseSessionBrowser();
    void RefreshSessionSearch();
    void StartRaceFromLobby();
    bool IsTrafficDebugOverlayVisible() const;
    float GetRouteFinishX() const { return RouteFinishX; }

    // --- Practice rival readouts --------------------------------------------
    bool HasPracticeRival() const { return PracticeBotController != nullptr; }

    /** Signed metres along the route: positive means the human is ahead. */
    int32 GetRivalGapMeters() const;

    /** One-line rival telemetry for the traffic debug overlay. */
    FString GetRivalDebugText() const;
    int32 GetSettingsSelection() const { return SettingsSelection; }
    int32 GetSettingsRowCount() const { return SettingsRowCount; }
    int32 GetMenuSelection() const { return MenuSelection; }
    FString GetSettingsLine(int32 Index) const;
    int32 GetActiveTrafficCount() const;
    int32 GetTrafficPoolSize() const;
    void StartSoloRace();
    void OpenSettings();
    void CloseSettings();
    void NavigateSettings(int32 Direction);
    void AdjustSelectedSetting(int32 Direction);
    void NavigateMenu(int32 Direction);
    void ConfirmMenuSelection();
    void RegisterTrafficCollision();
    void RegisterNearMiss();
    void ToggleRacePause();
    void RestartRace();
    void ReturnToMainMenu();

private:
    void FinishRace(class APlayerController* WinningPlayer, bool bWonByPracticeBot = false);
    void SyncNetworkRaceState();
    void SpawnPracticeBotForSoloRace();
    void DestroyPracticeBot();
    UPROPERTY(EditDefaultsOnly, Category = "Race", meta = (ClampMin = "0.0"))
    float CountdownDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Race", meta = (ClampMin = "0.0"))
    float GoMessageDuration = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Race")
    float RouteStartX = -299500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Race")
    float RouteFinishX = 300000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Race|Scoring", meta = (ClampMin = "0"))
    int32 NearMissBonusPoints = 250;

    UPROPERTY(EditDefaultsOnly, Category = "Race|Scoring", meta = (ClampMin = "0"))
    int32 CollisionPenaltyPoints = 750;

    UPROPERTY(Transient)
    TObjectPtr<class ATrafficDirector> LocalTrafficDirector;

    // The first bot milestone deliberately stays in standalone play.  It lets
    // us validate a server-controlled driver without changing the two-player
    // race flow that is already working in listen-server PIE.
    UPROPERTY(EditDefaultsOnly, Category = "Race|Practice Bot")
    bool bEnablePracticeBotInStandalone = true;

    UPROPERTY(EditDefaultsOnly, Category = "Race|Practice Bot", meta = (ClampMin = "0.0"))
    float PracticeBotStartAheadDistance = 9000.0f;

    UPROPERTY(Transient)
    TObjectPtr<class AOverlaneBotDriverController> PracticeBotController;

    UPROPERTY(Transient)
    TObjectPtr<class UOverlaneSettingsSaveGame> LocalSettings;

    UPROPERTY(Transient)
    TObjectPtr<class UOverlaneProgressSaveGame> LocalProgress;

    float RaceStartTime = 0.0f;
    float RaceFinishTime = 0.0f;
    float PauseStartTime = 0.0f;
    float AccumulatedPausedTime = 0.0f;
    float MaxSpeedKph = 0.0f;
    float CurrentCleanDriveSeconds = 0.0f;
    float LongestCleanDriveSeconds = 0.0f;
    float LastCollisionTime = -1000.0f;
    int32 CollisionCount = 0;
    int32 NearMissScore = 0;
    int32 RaceScore = 0;
    int32 WinningPlayerId = INDEX_NONE;
    class UOverlaneSessionSubsystem* GetSessionSubsystem() const;

    /** SOLO YARIS / OYUN KUR / OYUN BUL / AYARLAR. */
    static constexpr int32 MainMenuOptionCount = 4;

    /** Quality / VSync / frame cap / FOV / traffic debug / rival difficulty. */
    static constexpr int32 SettingsRowCount = 6;

    int32 SettingsSelection = 0;
    int32 MenuSelection = 0;
    int32 SessionSelection = 0;
    bool bShowingSessionBrowser = false;
    bool bInOnlineLobby = false;
    bool bShowingMainMenu = true;
    bool bShowingSettings = false;
    bool bSettingsOpenedFromPause = false;
    bool bSettingsAppliedToPawn = false;
    bool bNewPersonalBest = false;
    bool bPracticeBotWon = false;
    bool bRaceActive = false;
    bool bRaceFinished = false;
    bool bRacePaused = false;
    bool bMultiplayerRace = false;
};

#include "OverlaneHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneRaceGameState.h"
#include "OverlaneVehiclePawn.h"

void AOverlaneHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas || !GEngine)
    {
        return;
    }

    const AOverlaneGameModeBase* MenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    const AOverlaneRaceGameState* NetworkRaceState = GetWorld() ? GetWorld()->GetGameState<AOverlaneRaceGameState>() : nullptr;

    // The menu family is drawn before a pawn is required. The main menu and the
    // online browser both run while the local player has no vehicle at all, so
    // gating them on a driveable pawn would leave the player on a blank screen.
    if (MenuGameMode && MenuGameMode->IsSessionBrowserVisible())
    {
        FCanvasTextItem BrowserTitle(FVector2D((Canvas->ClipX * 0.5f) - 150.0f, 150.0f), FText::FromString(TEXT("OYUN BUL")), GEngine->GetLargeFont(), FLinearColor(0.2f, 0.75f, 1.0f));
        BrowserTitle.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(BrowserTitle);

        FCanvasTextItem StatusItem(FVector2D((Canvas->ClipX * 0.5f) - 150.0f, 200.0f), FText::FromString(MenuGameMode->GetSessionStatusText()), GEngine->GetMediumFont(), FLinearColor(1.0f, 0.82f, 0.2f));
        StatusItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(StatusItem);

        const int32 ResultCount = MenuGameMode->GetFoundSessionCount();
        for (int32 Index = 0; Index < ResultCount; ++Index)
        {
            const bool bSelected = MenuGameMode->GetSessionSelection() == Index;
            const FString RowText = FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), *MenuGameMode->GetFoundSessionLabel(Index));
            FCanvasTextItem RowItem(FVector2D((Canvas->ClipX * 0.5f) - 240.0f, 255.0f + (Index * 34.0f)), FText::FromString(RowText), GEngine->GetMediumFont(), bSelected ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor::White);
            RowItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(RowItem);
        }

        const bool bRefreshSelected = MenuGameMode->GetSessionSelection() >= ResultCount;
        FCanvasTextItem RefreshItem(FVector2D((Canvas->ClipX * 0.5f) - 240.0f, 255.0f + (ResultCount * 34.0f)), FText::FromString(FString::Printf(TEXT("%s TEKRAR ARA"), bRefreshSelected ? TEXT(">") : TEXT(" "))), GEngine->GetMediumFont(), bRefreshSelected ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor::White);
        RefreshItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(RefreshItem);

        FCanvasTextItem BrowserHint(FVector2D((Canvas->ClipX * 0.5f) - 240.0f, 460.0f), FText::FromString(TEXT("W / S: SEC    ENTER: KATIL    Q veya BACKSPACE: GERI")), GEngine->GetSmallFont(), FLinearColor::White);
        BrowserHint.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(BrowserHint);
        return;
    }

    if (MenuGameMode && MenuGameMode->IsOnlineLobbyVisible())
    {
        FCanvasTextItem LobbyTitle(FVector2D((Canvas->ClipX * 0.5f) - 90.0f, 170.0f), FText::FromString(TEXT("LOBI")), GEngine->GetLargeFont(), FLinearColor(0.2f, 0.75f, 1.0f));
        LobbyTitle.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(LobbyTitle);

        const FString PlayerCountText = FString::Printf(TEXT("OYUNCULAR: %d / 4"), MenuGameMode->GetConnectedPlayerCount());
        FCanvasTextItem PlayerCountItem(FVector2D((Canvas->ClipX * 0.5f) - 110.0f, 240.0f), FText::FromString(PlayerCountText), GEngine->GetMediumFont(), FLinearColor::White);
        PlayerCountItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(PlayerCountItem);

        FCanvasTextItem LobbyHint(FVector2D((Canvas->ClipX * 0.5f) - 175.0f, 300.0f), FText::FromString(TEXT("ENTER: YARISI BASLAT")), GEngine->GetMediumFont(), FLinearColor(0.35f, 1.0f, 0.55f));
        LobbyHint.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(LobbyHint);
        return;
    }

    if (MenuGameMode && MenuGameMode->IsMainMenuVisible() && !MenuGameMode->IsSettingsVisible())
    {
        FCanvasTextItem TitleItem(FVector2D((Canvas->ClipX * 0.5f) - 115.0f, 150.0f), FText::FromString(TEXT("OVERLANE")), GEngine->GetLargeFont(), FLinearColor(0.2f, 0.75f, 1.0f));
        TitleItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TitleItem);

        FCanvasTextItem SubtitleItem(FVector2D((Canvas->ClipX * 0.5f) - 120.0f, 205.0f), FText::FromString(TEXT("TRAFFIC SPRINT")), GEngine->GetMediumFont(), FLinearColor::White);
        SubtitleItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(SubtitleItem);

        static const TCHAR* MainMenuOptions[] = { TEXT("SOLO YARIS"), TEXT("ONLINE - OYUN KUR"), TEXT("ONLINE - OYUN BUL"), TEXT("AYARLAR") };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(MainMenuOptions); ++Index)
        {
            const bool bSelected = MenuGameMode->GetMenuSelection() == Index;
            const FString OptionText = FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), MainMenuOptions[Index]);
            FCanvasTextItem OptionItem(FVector2D((Canvas->ClipX * 0.5f) - 150.0f, 275.0f + (Index * 38.0f)), FText::FromString(OptionText), GEngine->GetMediumFont(), bSelected ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor::White);
            OptionItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(OptionItem);
        }

        FCanvasTextItem MenuHintItem(FVector2D((Canvas->ClipX * 0.5f) - 210.0f, 450.0f), FText::FromString(TEXT("A / D veya SOL / SAG: SEC     ENTER: ONAY")), GEngine->GetSmallFont(), FLinearColor::White);
        MenuHintItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(MenuHintItem);

        FCanvasTextItem OnlineHintItem(FVector2D((Canvas->ClipX * 0.5f) - 210.0f, 475.0f), FText::FromString(TEXT("ONLINE SU AN AYNI AGDAKI (LAN) OYUNLARI BULUR")), GEngine->GetSmallFont(), FLinearColor(0.65f, 0.65f, 0.65f));
        OnlineHintItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(OnlineHintItem);
        return;
    }

    if (MenuGameMode && MenuGameMode->IsSettingsVisible())
    {
        FCanvasTextItem SettingsTitle(FVector2D((Canvas->ClipX * 0.5f) - 90.0f, 160.0f), FText::FromString(TEXT("AYARLAR")), GEngine->GetLargeFont(), FLinearColor(0.2f, 0.75f, 1.0f));
        SettingsTitle.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(SettingsTitle);

        for (int32 Index = 0; Index < MenuGameMode->GetSettingsRowCount(); ++Index)
        {
            const bool bSelected = MenuGameMode->GetSettingsSelection() == Index;
            const FString SettingText = FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), *MenuGameMode->GetSettingsLine(Index));
            FCanvasTextItem SettingItem(FVector2D((Canvas->ClipX * 0.5f) - 190.0f, 245.0f + (Index * 38.0f)), FText::FromString(SettingText), GEngine->GetMediumFont(), bSelected ? FLinearColor(1.0f, 0.82f, 0.2f) : FLinearColor::White);
            SettingItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(SettingItem);
        }

        FCanvasTextItem HintItem(FVector2D((Canvas->ClipX * 0.5f) - 230.0f, 490.0f), FText::FromString(TEXT("YUKARI / ASAGI veya W / S: SEC   SOL / SAG veya A / D: DEGISTIR   Q veya BACKSPACE: GERI")), GEngine->GetSmallFont(), FLinearColor::White);
        HintItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(HintItem);
        return;
    }

    // Everything past this point is in-race telemetry and genuinely needs a car.
    const AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetOwningPlayerController() ? GetOwningPlayerController()->GetPawn() : nullptr);
    if (!VehiclePawn)
    {
        return;
    }

    const FString SpeedText = FString::Printf(TEXT("%03d KM/H"), FMath::RoundToInt(VehiclePawn->GetSpeedKph()));
    FCanvasTextItem TextItem(FVector2D(48.0f, 42.0f), FText::FromString(SpeedText), GEngine->GetLargeFont(), FLinearColor::White);
    TextItem.EnableShadow(FLinearColor::Black);
    Canvas->DrawItem(TextItem);

    const int32 TurboPercent = FMath::RoundToInt(VehiclePawn->GetBoostChargeRatio() * 100.0f);
    const FString TurboText = FString::Printf(TEXT("TURBO: %03d%%  [SHIFT]"), TurboPercent);
    FCanvasTextItem TurboItem(FVector2D(48.0f, 202.0f), FText::FromString(TurboText), GEngine->GetMediumFont(), VehiclePawn->IsBoostActive() ? FLinearColor(0.15f, 0.8f, 1.0f) : FLinearColor::White);
    TurboItem.EnableShadow(FLinearColor::Black);
    Canvas->DrawItem(TurboItem);

    const bool bUseNetworkRaceUi = NetworkRaceState && (!MenuGameMode || MenuGameMode->IsMultiplayerRace());
    if (bUseNetworkRaceUi)
    {
        const FString CountdownText = NetworkRaceState->GetCountdownText();
        if (!CountdownText.IsEmpty())
        {
            FCanvasTextItem CountdownItem(FVector2D((Canvas->ClipX * 0.5f) - 45.0f, 210.0f), FText::FromString(CountdownText), GEngine->GetLargeFont(), FLinearColor::White);
            CountdownItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(CountdownItem);
        }
        else if (!NetworkRaceState->IsRaceActive() && !NetworkRaceState->IsRaceFinished())
        {
            FCanvasTextItem WaitingItem(FVector2D((Canvas->ClipX * 0.5f) - 180.0f, 210.0f), FText::FromString(TEXT("HOST YARISI BASLATMAYI BEKLIYOR")), GEngine->GetMediumFont(), FLinearColor::White);
            WaitingItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(WaitingItem);
        }

        const FString DistanceText = FString::Printf(TEXT("KALAN: %d M"), NetworkRaceState->GetRemainingDistanceMeters(VehiclePawn));
        FCanvasTextItem DistanceItem(FVector2D(48.0f, 112.0f), FText::FromString(DistanceText), GEngine->GetMediumFont(), FLinearColor::White);
        DistanceItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(DistanceItem);

        const FString TimerText = FString::Printf(TEXT("SURE: %.1f SN"), NetworkRaceState->GetRaceElapsedSeconds());
        FCanvasTextItem TimerItem(FVector2D(48.0f, 142.0f), FText::FromString(TimerText), GEngine->GetMediumFont(), FLinearColor::White);
        TimerItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TimerItem);

        if (NetworkRaceState->IsRaceFinished())
        {
            const APlayerState* LocalPlayerState = GetOwningPlayerController() ? GetOwningPlayerController()->PlayerState : nullptr;
            const bool bLocalPlayerWon = NetworkRaceState->IsLocalPlayerWinner(LocalPlayerState);
            const FString ResultText = bLocalPlayerWon ? TEXT("SEN KAZANDIN!") : TEXT("DIGER OYUNCU KAZANDI");
            const FLinearColor ResultColor = bLocalPlayerWon ? FLinearColor(0.25f, 1.0f, 0.45f) : FLinearColor(1.0f, 0.78f, 0.28f);
            FCanvasTextItem ResultItem(FVector2D((Canvas->ClipX * 0.5f) - 175.0f, 260.0f), FText::FromString(ResultText), GEngine->GetLargeFont(), ResultColor);
            ResultItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(ResultItem);

            const FString FinishTimeText = FString::Printf(TEXT("BITIS: %.1f SN"), NetworkRaceState->GetRaceElapsedSeconds());
            FCanvasTextItem FinishTimeItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 310.0f), FText::FromString(FinishTimeText), GEngine->GetMediumFont(), FLinearColor::White);
            FinishTimeItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(FinishTimeItem);

            if (MenuGameMode)
            {
                FCanvasTextItem RestartHintItem(FVector2D((Canvas->ClipX * 0.5f) - 145.0f, 360.0f), FText::FromString(TEXT("HOST: R - YENIDEN BASLAT")), GEngine->GetSmallFont(), FLinearColor::White);
                RestartHintItem.EnableShadow(FLinearColor::Black);
                Canvas->DrawItem(RestartHintItem);
            }
        }
    }

    if (!bUseNetworkRaceUi)
    {
        if (const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
        {
            const FString CountdownText = GameMode->GetRaceCountdownText();
            if (!CountdownText.IsEmpty())
            {
                FCanvasTextItem CountdownItem(FVector2D((Canvas->ClipX * 0.5f) - 45.0f, 210.0f), FText::FromString(CountdownText), GEngine->GetLargeFont(), FLinearColor::White);
                CountdownItem.EnableShadow(FLinearColor::Black);
                Canvas->DrawItem(CountdownItem);
            }

            if (GameMode->IsRacePaused())
            {
                FCanvasTextItem PauseItem(FVector2D((Canvas->ClipX * 0.5f) - 125.0f, 210.0f), FText::FromString(TEXT("DURAKLATILDI")), GEngine->GetLargeFont(), FLinearColor(1.0f, 0.82f, 0.2f));
                PauseItem.EnableShadow(FLinearColor::Black);
                Canvas->DrawItem(PauseItem);

                static const TCHAR* PauseOptions[] = { TEXT("DEVAM"), TEXT("YENIDEN BASLAT"), TEXT("ANA MENU"), TEXT("AYARLAR") };
                static const float PauseOptionOffsets[] = { -310.0f, -140.0f, 75.0f, 220.0f };
                for (int32 Index = 0; Index < UE_ARRAY_COUNT(PauseOptions); ++Index)
                {
                    const bool bSelected = GameMode->GetMenuSelection() == Index;
                    const FString PauseOptionText = FString::Printf(TEXT("[%s] %s"), bSelected ? TEXT(">") : TEXT(" "), PauseOptions[Index]);
                    FCanvasTextItem PauseOptionItem(FVector2D((Canvas->ClipX * 0.5f) + PauseOptionOffsets[Index], 285.0f), FText::FromString(PauseOptionText), GEngine->GetSmallFont(), bSelected ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor::White);
                    PauseOptionItem.EnableShadow(FLinearColor::Black);
                    Canvas->DrawItem(PauseOptionItem);
                }

                FCanvasTextItem PauseHintItem(FVector2D((Canvas->ClipX * 0.5f) - 170.0f, 340.0f), FText::FromString(TEXT("SOL / SAG veya A / D - SEC     ENTER - ONAY")), GEngine->GetSmallFont(), FLinearColor::White);
                PauseHintItem.EnableShadow(FLinearColor::Black);
                Canvas->DrawItem(PauseHintItem);
            }
        }
    }

    const FString NearMissText = FString::Printf(TEXT("YAKIN GECIS: %d"), VehiclePawn->GetNearMissCount());
    FCanvasTextItem NearMissCountItem(FVector2D(48.0f, 82.0f), FText::FromString(NearMissText), GEngine->GetMediumFont(), FLinearColor::White);
    NearMissCountItem.EnableShadow(FLinearColor::Black);
    Canvas->DrawItem(NearMissCountItem);

    if (!bUseNetworkRaceUi)
    {
        if (const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
        {
        const FString DistanceText = FString::Printf(TEXT("KALAN: %d M"), GameMode->GetRemainingDistanceMeters());
        FCanvasTextItem DistanceItem(FVector2D(48.0f, 112.0f), FText::FromString(DistanceText), GEngine->GetMediumFont(), FLinearColor::White);
        DistanceItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(DistanceItem);

        const FString TimerText = FString::Printf(TEXT("SURE: %.1f SN"), GameMode->GetRaceElapsedSeconds());
        FCanvasTextItem TimerItem(FVector2D(48.0f, 142.0f), FText::FromString(TimerText), GEngine->GetMediumFont(), FLinearColor::White);
        TimerItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TimerItem);

        const FString ScoreText = FString::Printf(TEXT("SKOR: %d"), GameMode->GetRaceScore());
        FCanvasTextItem ScoreItem(FVector2D(48.0f, 172.0f), FText::FromString(ScoreText), GEngine->GetMediumFont(), FLinearColor(0.35f, 1.0f, 0.55f));
        ScoreItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(ScoreItem);

        const FString ResultText = GameMode->GetRaceResultText();
        if (!ResultText.IsEmpty())
        {
            const FString WinnerText = GameMode->GetRaceWinnerText();
            const FLinearColor WinnerColor = GameMode->DidPracticeBotWin() ? FLinearColor(1.0f, 0.78f, 0.28f) : FLinearColor(0.25f, 1.0f, 0.45f);
            FCanvasTextItem WinnerItem(FVector2D((Canvas->ClipX * 0.5f) - 155.0f, 210.0f), FText::FromString(WinnerText), GEngine->GetLargeFont(), WinnerColor);
            WinnerItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(WinnerItem);

            FCanvasTextItem ResultItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 246.0f), FText::FromString(ResultText), GEngine->GetMediumFont(), FLinearColor::White);
            ResultItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(ResultItem);

            const FString ResultScoreText = FString::Printf(TEXT("SKOR: %d"), GameMode->GetRaceScore());
            FCanvasTextItem ResultScoreItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 278.0f), FText::FromString(ResultScoreText), GEngine->GetMediumFont(), FLinearColor(0.35f, 1.0f, 0.55f));
            ResultScoreItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(ResultScoreItem);

            const FString ResultNearMissText = FString::Printf(TEXT("YAKIN GECIS: %d  (+%d)"), VehiclePawn->GetNearMissCount(), GameMode->GetNearMissScore());
            FCanvasTextItem ResultNearMissItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 310.0f), FText::FromString(ResultNearMissText), GEngine->GetMediumFont(), FLinearColor::White);
            ResultNearMissItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(ResultNearMissItem);

            const FString CollisionText = FString::Printf(TEXT("CARPISMA: %d  (-%d)"), GameMode->GetCollisionCount(), GameMode->GetCollisionPenalty());
            FCanvasTextItem CollisionItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 342.0f), FText::FromString(CollisionText), GEngine->GetMediumFont(), FLinearColor::White);
            CollisionItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(CollisionItem);

            const FString MaxSpeedText = FString::Printf(TEXT("MAX HIZ: %d KM/H"), GameMode->GetMaxSpeedKph());
            FCanvasTextItem MaxSpeedItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 374.0f), FText::FromString(MaxSpeedText), GEngine->GetMediumFont(), FLinearColor::White);
            MaxSpeedItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(MaxSpeedItem);

            const FString CleanDriveText = FString::Printf(TEXT("EN TEMIZ: %.1f SN"), GameMode->GetLongestCleanDriveSeconds());
            FCanvasTextItem CleanDriveItem(FVector2D((Canvas->ClipX * 0.5f) - 105.0f, 406.0f), FText::FromString(CleanDriveText), GEngine->GetMediumFont(), FLinearColor::White);
            CleanDriveItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(CleanDriveItem);

            const FString BestTimeText = GameMode->IsNewPersonalBest()
                ? FString::Printf(TEXT("YENI EN IYI!  SKOR REKORU: %d"), GameMode->GetBestSoloScore())
                : FString::Printf(TEXT("EN IYI: %.1f SN  |  SKOR: %d"), GameMode->GetBestSoloTimeSeconds(), GameMode->GetBestSoloScore());
            FCanvasTextItem BestTimeItem(FVector2D((Canvas->ClipX * 0.5f) - 165.0f, 446.0f), FText::FromString(BestTimeText), GEngine->GetMediumFont(), GameMode->IsNewPersonalBest() ? FLinearColor(0.25f, 1.0f, 0.45f) : FLinearColor::White);
            BestTimeItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(BestTimeItem);

            FCanvasTextItem RestartItem(FVector2D((Canvas->ClipX * 0.5f) - 110.0f, 490.0f), FText::FromString(TEXT("R - YENIDEN BASLA")), GEngine->GetMediumFont(), FLinearColor::White);
            RestartItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(RestartItem);

            FCanvasTextItem MenuResultItem(FVector2D((Canvas->ClipX * 0.5f) - 80.0f, 530.0f), FText::FromString(TEXT("M - ANA MENU")), GEngine->GetMediumFont(), FLinearColor::White);
            MenuResultItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(MenuResultItem);
        }
        }
    }

    // The rival gap only exists in solo play: a practice bot never spawns in a
    // networked race, so drawing it there would show a permanently stale value.
    if (MenuGameMode && !bUseNetworkRaceUi && MenuGameMode->HasPracticeRival())
    {
        const int32 GapMeters = MenuGameMode->GetRivalGapMeters();
        const FString RivalText = GapMeters >= 0
            ? FString::Printf(TEXT("RAKIP: %d M GERIDE"), GapMeters)
            : FString::Printf(TEXT("RAKIP: %d M ONDE"), -GapMeters);
        FCanvasTextItem RivalItem(FVector2D(48.0f, 232.0f), FText::FromString(RivalText), GEngine->GetMediumFont(),
            GapMeters >= 0 ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(1.0f, 0.45f, 0.28f));
        RivalItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(RivalItem);
    }

    if (VehiclePawn->IsTrafficImpactFeedbackActive())
    {
        const bool bRivalContact = VehiclePawn->IsRivalContactFeedbackActive();
        FCanvasTextItem ImpactItem(FVector2D((Canvas->ClipX * 0.5f) - 165.0f, 105.0f),
            FText::FromString(bRivalContact ? TEXT("RAKIP TEMASI") : TEXT("TRAFIK CARPISMASI")),
            GEngine->GetLargeFont(), VehiclePawn->GetTrafficImpactFeedbackColor());
        ImpactItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(ImpactItem);
    }

    if (VehiclePawn->IsNearMissFeedbackActive())
    {
        FCanvasTextItem NearMissItem(FVector2D((Canvas->ClipX * 0.5f) - 125.0f, 150.0f), FText::FromString(TEXT("YAKIN GECIS +1")), GEngine->GetLargeFont(), VehiclePawn->GetNearMissFeedbackColor());
        NearMissItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(NearMissItem);
    }

    // Deliberately NOT gated on the game mode: it is null on a client, which is
    // precisely where the prediction error needs to be readable.
    if (VehiclePawn->HasServerGhost() && AOverlaneVehiclePawn::IsCorrectionDebugEnabled())
    {
        // Both numbers, labelled distinctly and on purpose. LAG is the client's
        // legitimate lead over the server and SHOULD be large and grow with ping.
        // ERR is the reconciliation error for the SAME sequence and should be
        // near zero. Showing only one of them means a tester reads the wrong one
        // and derives a correction threshold from a number that is not an error.
        const FOverlaneReconcileSample& Sample = VehiclePawn->GetLastReconcileSample();
        const FString NetDebugText = FString::Printf(
            TEXT("NET  LAG %+.0f cm   ERR %+.1f / %+.1f cm  YAW %+.3f  HIZ %+.1f  DERINLIK %d  ACK %d  KACAK %d"),
            VehiclePawn->GetServerLagLongitudinalCm(),
            Sample.ErrorLongitudinalCm,
            Sample.ErrorLateralCm,
            Sample.ErrorYawDegrees,
            Sample.ErrorSpeedCms,
            Sample.UnackedDepth,
            VehiclePawn->GetAckCount(),
            VehiclePawn->GetRingMissCount());
        FCanvasTextItem NetDebugItem(FVector2D(48.0f, Canvas->ClipY - 76.0f), FText::FromString(NetDebugText), GEngine->GetSmallFont(), FLinearColor(0.4f, 0.85f, 1.0f));
        NetDebugItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(NetDebugItem);
    }

    if (MenuGameMode && MenuGameMode->IsTrafficDebugOverlayVisible())
    {
        const FString TrafficDebugText = FString::Printf(TEXT("DEBUG TRAFIK: %d / %d"), MenuGameMode->GetActiveTrafficCount(), MenuGameMode->GetTrafficPoolSize());
        FCanvasTextItem TrafficDebugItem(FVector2D(48.0f, Canvas->ClipY - 52.0f), FText::FromString(TrafficDebugText), GEngine->GetSmallFont(), FLinearColor(0.35f, 1.0f, 0.55f));
        TrafficDebugItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TrafficDebugItem);

        const FString RivalDebugText = MenuGameMode->GetRivalDebugText();
        if (!RivalDebugText.IsEmpty())
        {
            FCanvasTextItem RivalDebugItem(FVector2D(48.0f, Canvas->ClipY - 28.0f), FText::FromString(RivalDebugText), GEngine->GetSmallFont(), FLinearColor(1.0f, 0.78f, 0.28f));
            RivalDebugItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(RivalDebugItem);
        }
    }
}

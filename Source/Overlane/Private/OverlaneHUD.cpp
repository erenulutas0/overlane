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

    const AOverlaneVehiclePawn* VehiclePawn = Cast<AOverlaneVehiclePawn>(GetOwningPlayerController() ? GetOwningPlayerController()->GetPawn() : nullptr);
    if (!Canvas || !VehiclePawn || !GEngine)
    {
        return;
    }

    const AOverlaneGameModeBase* MenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    const AOverlaneRaceGameState* NetworkRaceState = GetWorld() ? GetWorld()->GetGameState<AOverlaneRaceGameState>() : nullptr;
    if (MenuGameMode && MenuGameMode->IsMainMenuVisible() && !MenuGameMode->IsSettingsVisible())
    {
        FCanvasTextItem TitleItem(FVector2D((Canvas->ClipX * 0.5f) - 115.0f, 160.0f), FText::FromString(TEXT("OVERLANE")), GEngine->GetLargeFont(), FLinearColor(0.2f, 0.75f, 1.0f));
        TitleItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TitleItem);

        FCanvasTextItem SubtitleItem(FVector2D((Canvas->ClipX * 0.5f) - 120.0f, 215.0f), FText::FromString(TEXT("TRAFFIC SPRINT")), GEngine->GetMediumFont(), FLinearColor::White);
        SubtitleItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(SubtitleItem);

        const bool bSoloSelected = MenuGameMode->GetMenuSelection() == 0;
        FCanvasTextItem SoloItem(FVector2D((Canvas->ClipX * 0.5f) - 230.0f, 315.0f), FText::FromString(TEXT("[>] SOLO YARIS")), GEngine->GetMediumFont(), bSoloSelected ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor::White);
        SoloItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(SoloItem);

        FCanvasTextItem SettingsItem(FVector2D((Canvas->ClipX * 0.5f) + 45.0f, 315.0f), FText::FromString(TEXT("[*] AYARLAR")), GEngine->GetMediumFont(), !bSoloSelected ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor::White);
        SettingsItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(SettingsItem);

        FCanvasTextItem MenuHintItem(FVector2D((Canvas->ClipX * 0.5f) - 170.0f, 375.0f), FText::FromString(TEXT("SOL / SAG veya A / D - SEC     ENTER - ONAY")), GEngine->GetSmallFont(), FLinearColor::White);
        MenuHintItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(MenuHintItem);
        return;
    }

    if (MenuGameMode && MenuGameMode->IsSettingsVisible())
    {
        FCanvasTextItem SettingsTitle(FVector2D((Canvas->ClipX * 0.5f) - 90.0f, 160.0f), FText::FromString(TEXT("AYARLAR")), GEngine->GetLargeFont(), FLinearColor(0.2f, 0.75f, 1.0f));
        SettingsTitle.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(SettingsTitle);

        for (int32 Index = 0; Index < 5; ++Index)
        {
            const bool bSelected = MenuGameMode->GetSettingsSelection() == Index;
            const FString SettingText = FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), *MenuGameMode->GetSettingsLine(Index));
            FCanvasTextItem SettingItem(FVector2D((Canvas->ClipX * 0.5f) - 190.0f, 245.0f + (Index * 38.0f)), FText::FromString(SettingText), GEngine->GetMediumFont(), bSelected ? FLinearColor(1.0f, 0.82f, 0.2f) : FLinearColor::White);
            SettingItem.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(SettingItem);
        }

        FCanvasTextItem HintItem(FVector2D((Canvas->ClipX * 0.5f) - 220.0f, 470.0f), FText::FromString(TEXT("YUKARI / ASAGI veya W / S: SEC   SOL / SAG veya A / D: DEGISTIR   ESC: GERI")), GEngine->GetSmallFont(), FLinearColor::White);
        HintItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(HintItem);
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

    if (VehiclePawn->IsTrafficImpactFeedbackActive())
    {
        FCanvasTextItem ImpactItem(FVector2D((Canvas->ClipX * 0.5f) - 165.0f, 105.0f), FText::FromString(TEXT("TRAFIK CARPISMASI")), GEngine->GetLargeFont(), VehiclePawn->GetTrafficImpactFeedbackColor());
        ImpactItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(ImpactItem);
    }

    if (VehiclePawn->IsNearMissFeedbackActive())
    {
        FCanvasTextItem NearMissItem(FVector2D((Canvas->ClipX * 0.5f) - 125.0f, 150.0f), FText::FromString(TEXT("YAKIN GECIS +1")), GEngine->GetLargeFont(), VehiclePawn->GetNearMissFeedbackColor());
        NearMissItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(NearMissItem);
    }

    if (MenuGameMode && MenuGameMode->IsTrafficDebugOverlayVisible())
    {
        const FString TrafficDebugText = FString::Printf(TEXT("DEBUG TRAFIK: %d / %d"), MenuGameMode->GetActiveTrafficCount(), MenuGameMode->GetTrafficPoolSize());
        FCanvasTextItem TrafficDebugItem(FVector2D(48.0f, Canvas->ClipY - 52.0f), FText::FromString(TrafficDebugText), GEngine->GetSmallFont(), FLinearColor(0.35f, 1.0f, 0.55f));
        TrafficDebugItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TrafficDebugItem);
    }
}

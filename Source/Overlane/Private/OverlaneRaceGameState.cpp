#include "OverlaneRaceGameState.h"

#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

void AOverlaneRaceGameState::SetRaceFlow(bool bInRaceActive, bool bInRaceFinished, bool bInRacePaused, float InRaceStartTime, float InRaceFinishTime, float InRouteStartX, float InRouteFinishX, int32 InWinningPlayerId)
{
    bRaceActive = bInRaceActive;
    bRaceFinished = bInRaceFinished;
    bRacePaused = bInRacePaused;
    RaceStartServerTime = InRaceStartTime;
    RaceFinishServerTime = InRaceFinishTime;
    RouteStartX = InRouteStartX;
    RouteFinishX = InRouteFinishX;
    WinningPlayerId = InWinningPlayerId;
}

FString AOverlaneRaceGameState::GetCountdownText() const
{
    if (bRaceActive || bRaceFinished || RaceStartServerTime <= 0.0f)
    {
        return FString();
    }

    const float SecondsRemaining = RaceStartServerTime - GetServerWorldTimeSeconds();
    return SecondsRemaining > 0.0f ? FString::FromInt(FMath::CeilToInt(SecondsRemaining)) : TEXT("GO!");
}

float AOverlaneRaceGameState::GetRaceElapsedSeconds() const
{
    if (RaceStartServerTime <= 0.0f)
    {
        return 0.0f;
    }

    const float EndTime = bRaceFinished ? RaceFinishServerTime : GetServerWorldTimeSeconds();
    return FMath::Max(0.0f, EndTime - RaceStartServerTime);
}

int32 AOverlaneRaceGameState::GetRemainingDistanceMeters(const APawn* PlayerPawn) const
{
    const float PlayerX = PlayerPawn ? PlayerPawn->GetActorLocation().X : RouteStartX;
    return FMath::CeilToInt(FMath::Max(0.0f, RouteFinishX - PlayerX) / 100.0f);
}

bool AOverlaneRaceGameState::IsLocalPlayerWinner(const APlayerState* LocalPlayerState) const
{
    return LocalPlayerState && WinningPlayerId != INDEX_NONE && LocalPlayerState->GetPlayerId() == WinningPlayerId;
}

void AOverlaneRaceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AOverlaneRaceGameState, bRaceActive);
    DOREPLIFETIME(AOverlaneRaceGameState, bRaceFinished);
    DOREPLIFETIME(AOverlaneRaceGameState, bRacePaused);
    DOREPLIFETIME(AOverlaneRaceGameState, RaceStartServerTime);
    DOREPLIFETIME(AOverlaneRaceGameState, RaceFinishServerTime);
    DOREPLIFETIME(AOverlaneRaceGameState, RouteStartX);
    DOREPLIFETIME(AOverlaneRaceGameState, RouteFinishX);
    DOREPLIFETIME(AOverlaneRaceGameState, WinningPlayerId);
}

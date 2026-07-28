#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "OverlaneRaceGameState.generated.h"

/** Replicated, client-readable race flow for the first listen-server prototype. */
UCLASS()
class OVERLANE_API AOverlaneRaceGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    void SetRaceFlow(bool bInRaceActive, bool bInRaceFinished, bool bInRacePaused, float InRaceStartTime, float InRaceFinishTime, float InRouteStartX, float InRouteFinishX, int32 InWinningPlayerId);

    bool IsRaceActive() const { return bRaceActive; }
    bool IsRaceFinished() const { return bRaceFinished; }
    bool IsRacePaused() const { return bRacePaused; }
    FString GetCountdownText() const;
    float GetRaceElapsedSeconds() const;
    int32 GetRemainingDistanceMeters(const APawn* PlayerPawn) const;
    bool HasWinner() const { return WinningPlayerId != INDEX_NONE; }
    bool IsLocalPlayerWinner(const class APlayerState* LocalPlayerState) const;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UPROPERTY(Replicated)
    bool bRaceActive = false;

    UPROPERTY(Replicated)
    bool bRaceFinished = false;

    UPROPERTY(Replicated)
    bool bRacePaused = false;

    UPROPERTY(Replicated)
    float RaceStartServerTime = 0.0f;

    UPROPERTY(Replicated)
    float RaceFinishServerTime = 0.0f;

    UPROPERTY(Replicated)
    float RouteStartX = -299500.0f;

    UPROPERTY(Replicated)
    float RouteFinishX = 300000.0f;

    // PlayerState's network-stable id of the first player to cross the line.
    // Keeping only the id makes the result readable by every client without
    // requiring the authoritative GameMode to exist there.
    UPROPERTY(Replicated)
    int32 WinningPlayerId = INDEX_NONE;
};

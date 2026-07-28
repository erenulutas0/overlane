#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OverlaneSessionSubsystem.generated.h"

UENUM()
enum class EOverlaneSessionPhase : uint8
{
    Idle,
    Creating,
    Searching,
    Joining,
    Leaving,
    Failed
};

/**
 * Host / find / join for Overlane races.
 *
 * This is deliberately written against the classic IOnlineSubsystem interface
 * rather than the newer OnlineServices API, because OnlineSubsystemSteam is the
 * mature implementation we intend to switch to in Phase 7.  Swapping LAN
 * discovery (OnlineSubsystemNull) for Steam should then be a config change plus
 * a bUseLanMatch flip, not a rewrite of this class.
 *
 * All state is deliberately simple and pollable: the HUD is immediate-mode
 * Canvas, so it reads status strings every frame instead of binding delegates.
 */
UCLASS()
class OVERLANE_API UOverlaneSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Creates a listen-server session and travels this machine to the race map. */
    void HostSession(int32 MaxPlayers = 4);

    /** Starts an asynchronous search; results are polled via GetFoundSessionCount(). */
    void FindSessions();

    /** Joins a result from the last successful search and client-travels to the host. */
    void JoinFoundSession(int32 SearchResultIndex);

    /** Destroys the local session, if any. Safe to call when there is none. */
    void LeaveSession();

    EOverlaneSessionPhase GetPhase() const { return Phase; }
    bool IsBusy() const;

    /** Single line of user-facing Turkish status for the Canvas HUD. */
    const FString& GetStatusText() const { return StatusText; }

    int32 GetFoundSessionCount() const;
    FString GetFoundSessionLabel(int32 Index) const;

    /** True while LAN discovery is used; false once a real Steam session is configured. */
    bool IsLanMatch() const { return bUseLanMatch; }

private:
    IOnlineSessionPtr GetSessionInterface() const;
    FUniqueNetIdPtr GetLocalPlayerNetId() const;

    void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleFindSessionsComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

    void SetPhase(EOverlaneSessionPhase InPhase, const FString& InStatusText);
    void ClearDelegateHandles();

    FDelegateHandle CreateSessionCompleteHandle;
    FDelegateHandle FindSessionsCompleteHandle;
    FDelegateHandle JoinSessionCompleteHandle;
    FDelegateHandle DestroySessionCompleteHandle;

    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    EOverlaneSessionPhase Phase = EOverlaneSessionPhase::Idle;
    FString StatusText;

    /**
     * Set when the caller asked to host but an old session had to be destroyed
     * first, so the destroy callback knows to continue into creation.
     */
    bool bHostAfterDestroy = false;
    int32 PendingMaxPlayers = 4;

    /**
     * OnlineSubsystemNull only discovers sessions over LAN broadcast, so this is
     * true for now.  Phase 7 sets it false alongside the Steam app id.
     */
    bool bUseLanMatch = true;
};

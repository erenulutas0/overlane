#include "OverlaneSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

// SETTING_MAPNAME / SEARCH_PRESENCE live in the OnlineBase plugin in UE 5.8;
// OnlineSessionSettings.h only mentions them in comments.
#include "Online/OnlineSessionNames.h"

namespace
{
    /**
     * The one race map.  When a hand-authored production map replaces it this
     * becomes a data-asset lookup rather than a literal.
     */
    const TCHAR* OverlaneRaceMapUrl = TEXT("/Game/Maps/L_VehicleHandlingTest?listen");

    /** Advertised so the browser can show something more useful than an index. */
    const FName OverlaneHostNameKey = FName(TEXT("OVERLANE_HOSTNAME"));
}

void UOverlaneSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StatusText = TEXT("CEVRIMICI HAZIR");
}

void UOverlaneSessionSubsystem::Deinitialize()
{
    ClearDelegateHandles();
    Super::Deinitialize();
}

IOnlineSessionPtr UOverlaneSessionSubsystem::GetSessionInterface() const
{
    IOnlineSubsystem* const Subsystem = Online::GetSubsystem(GetWorld());
    return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
}

FUniqueNetIdPtr UOverlaneSessionSubsystem::GetLocalPlayerNetId() const
{
    const UGameInstance* const GameInstance = GetGameInstance();
    const ULocalPlayer* const LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
    return LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId() : nullptr;
}

bool UOverlaneSessionSubsystem::IsBusy() const
{
    return Phase == EOverlaneSessionPhase::Creating
        || Phase == EOverlaneSessionPhase::Searching
        || Phase == EOverlaneSessionPhase::Joining
        || Phase == EOverlaneSessionPhase::Leaving;
}

void UOverlaneSessionSubsystem::SetPhase(EOverlaneSessionPhase InPhase, const FString& InStatusText)
{
    Phase = InPhase;
    StatusText = InStatusText;
}

void UOverlaneSessionSubsystem::ClearDelegateHandles()
{
    const IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        return;
    }

    if (CreateSessionCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        CreateSessionCompleteHandle.Reset();
    }
    if (FindSessionsCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        FindSessionsCompleteHandle.Reset();
    }
    if (JoinSessionCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        JoinSessionCompleteHandle.Reset();
    }
    if (DestroySessionCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
        DestroySessionCompleteHandle.Reset();
    }
}

void UOverlaneSessionSubsystem::HostSession(int32 MaxPlayers)
{
    const IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("CEVRIMICI SERVIS YOK"));
        return;
    }

    if (IsBusy())
    {
        return;
    }

    PendingMaxPlayers = FMath::Clamp(MaxPlayers, 2, 4);

    // A stale session from a previous race would make CreateSession fail outright,
    // so destroy it first and resume hosting from the destroy callback.
    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        bHostAfterDestroy = true;
        LeaveSession();
        return;
    }

    const FUniqueNetIdPtr LocalNetId = GetLocalPlayerNetId();
    if (!LocalNetId.IsValid())
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OYUNCU KIMLIGI YOK"));
        return;
    }

    FOnlineSessionSettings Settings;
    Settings.NumPublicConnections = PendingMaxPlayers;
    Settings.NumPrivateConnections = 0;
    Settings.bIsLANMatch = bUseLanMatch;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bAllowJoinViaPresence = true;
    Settings.bUsesPresence = !bUseLanMatch;
    Settings.bUseLobbiesIfAvailable = !bUseLanMatch;
    Settings.bAllowInvites = true;
    Settings.bIsDedicated = false;

    const FString HostLabel = FPlatformProcess::ComputerName();
    Settings.Set(OverlaneHostNameKey, HostLabel, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(SETTING_MAPNAME, FString(TEXT("L_VehicleHandlingTest")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &UOverlaneSessionSubsystem::HandleCreateSessionComplete));

    SetPhase(EOverlaneSessionPhase::Creating, TEXT("OYUN KURULUYOR..."));

    if (!SessionInterface->CreateSession(*LocalNetId, NAME_GameSession, Settings))
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        CreateSessionCompleteHandle.Reset();
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OYUN KURULAMADI"));
    }
}

void UOverlaneSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (const IOnlineSessionPtr SessionInterface = GetSessionInterface())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
    }
    CreateSessionCompleteHandle.Reset();

    if (!bWasSuccessful)
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OYUN KURULAMADI"));
        return;
    }

    UWorld* const World = GetWorld();
    if (!World)
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("HARITA ACILAMADI"));
        return;
    }

    SetPhase(EOverlaneSessionPhase::Idle, TEXT("LOBI ACILDI"));

    // Absolute travel so the host becomes a listen server the search can find.
    World->ServerTravel(OverlaneRaceMapUrl, true);
}

void UOverlaneSessionSubsystem::FindSessions()
{
    const IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("CEVRIMICI SERVIS YOK"));
        return;
    }

    if (IsBusy())
    {
        return;
    }

    const FUniqueNetIdPtr LocalNetId = GetLocalPlayerNetId();
    if (!LocalNetId.IsValid())
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OYUNCU KIMLIGI YOK"));
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = bUseLanMatch;
    SessionSearch->MaxSearchResults = 32;
    SessionSearch->PingBucketSize = 50;

    // Lobby search is a Steam concept and has no meaning for a LAN broadcast
    // query, so it is only set once we are off OnlineSubsystemNull.  UE 5.8 has
    // no SEARCH_PRESENCE; SEARCH_LOBBIES is the current Steam lobby discovery key.
    if (!bUseLanMatch)
    {
        SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    }

    FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UOverlaneSessionSubsystem::HandleFindSessionsComplete));

    SetPhase(EOverlaneSessionPhase::Searching, TEXT("OYUNLAR ARANIYOR..."));

    if (!SessionInterface->FindSessions(*LocalNetId, SessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        FindSessionsCompleteHandle.Reset();
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("ARAMA BASLATILAMADI"));
    }
}

void UOverlaneSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
    if (const IOnlineSessionPtr SessionInterface = GetSessionInterface())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
    }
    FindSessionsCompleteHandle.Reset();

    const int32 ResultCount = GetFoundSessionCount();
    if (!bWasSuccessful)
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("ARAMA BASARISIZ"));
        return;
    }

    SetPhase(
        EOverlaneSessionPhase::Idle,
        ResultCount > 0
            ? FString::Printf(TEXT("%d OYUN BULUNDU"), ResultCount)
            : FString(TEXT("OYUN BULUNAMADI")));
}

int32 UOverlaneSessionSubsystem::GetFoundSessionCount() const
{
    return SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
}

FString UOverlaneSessionSubsystem::GetFoundSessionLabel(int32 Index) const
{
    if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(Index))
    {
        return FString();
    }

    const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];

    FString HostLabel;
    if (!Result.Session.SessionSettings.Get(OverlaneHostNameKey, HostLabel) || HostLabel.IsEmpty())
    {
        HostLabel = Result.Session.OwningUserName.IsEmpty() ? TEXT("BILINMEYEN HOST") : Result.Session.OwningUserName;
    }

    const int32 MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
    const int32 UsedSlots = FMath::Max(0, MaxPlayers - Result.Session.NumOpenPublicConnections);

    return FString::Printf(TEXT("%s   %d/%d   %d ms"), *HostLabel, UsedSlots, MaxPlayers, Result.PingInMs);
}

void UOverlaneSessionSubsystem::JoinFoundSession(int32 SearchResultIndex)
{
    const IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
    {
        return;
    }

    if (IsBusy())
    {
        return;
    }

    const FUniqueNetIdPtr LocalNetId = GetLocalPlayerNetId();
    if (!LocalNetId.IsValid())
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OYUNCU KIMLIGI YOK"));
        return;
    }

    JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UOverlaneSessionSubsystem::HandleJoinSessionComplete));

    SetPhase(EOverlaneSessionPhase::Joining, TEXT("BAGLANILIYOR..."));

    if (!SessionInterface->JoinSession(*LocalNetId, NAME_GameSession, SessionSearch->SearchResults[SearchResultIndex]))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        JoinSessionCompleteHandle.Reset();
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("BAGLANILAMADI"));
    }
}

void UOverlaneSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    const IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
    }
    JoinSessionCompleteHandle.Reset();

    if (!SessionInterface.IsValid() || Result != EOnJoinSessionCompleteResult::Success)
    {
        SetPhase(
            EOverlaneSessionPhase::Failed,
            Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("OYUN DOLU") : TEXT("BAGLANILAMADI"));
        return;
    }

    FString ConnectString;
    if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("ADRES COZULEMEDI"));
        return;
    }

    APlayerController* const LocalController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!LocalController)
    {
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OYUNCU BULUNAMADI"));
        return;
    }

    SetPhase(EOverlaneSessionPhase::Idle, TEXT("BAGLANDI"));
    LocalController->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
}

void UOverlaneSessionSubsystem::LeaveSession()
{
    const IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid() || SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
    {
        // Nothing to destroy: if this was the cleanup half of a host request,
        // go straight to creating the new session.
        if (bHostAfterDestroy)
        {
            bHostAfterDestroy = false;
            HostSession(PendingMaxPlayers);
        }
        return;
    }

    DestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOverlaneSessionSubsystem::HandleDestroySessionComplete));

    SetPhase(EOverlaneSessionPhase::Leaving, TEXT("OTURUM KAPATILIYOR..."));

    if (!SessionInterface->DestroySession(NAME_GameSession))
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
        DestroySessionCompleteHandle.Reset();
        bHostAfterDestroy = false;
        SetPhase(EOverlaneSessionPhase::Failed, TEXT("OTURUM KAPATILAMADI"));
    }
}

void UOverlaneSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (const IOnlineSessionPtr SessionInterface = GetSessionInterface())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
    }
    DestroySessionCompleteHandle.Reset();

    SetPhase(
        bWasSuccessful ? EOverlaneSessionPhase::Idle : EOverlaneSessionPhase::Failed,
        bWasSuccessful ? TEXT("CEVRIMICI HAZIR") : TEXT("OTURUM KAPATILAMADI"));

    if (bHostAfterDestroy)
    {
        bHostAfterDestroy = false;
        if (bWasSuccessful)
        {
            HostSession(PendingMaxPlayers);
        }
    }
}

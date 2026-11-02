#include "Multiplayer/THSessionSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TacticalHorror.h"

IOnlineSessionPtr UTHSessionSubsystem::GetSessionInterface() const
{
	return Online::GetSessionInterface(GetWorld());
}

// Host

void UTHSessionSubsystem::HostSession(const FString& MapPath, int32 MaxPlayers, bool bLAN)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogTacticalHorror, Warning, TEXT("HostSession: no online session interface"));
		OnHostComplete.Broadcast(false);
		return;
	}

	PendingHostMap = MapPath;
	PendingMaxPlayers = MaxPlayers;
	bPendingLAN = bLAN;

	// A session left over from a previous game blocks creating a new one
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bHostAfterDestroy = true;
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UTHSessionSubsystem::HandleDestroyComplete));
		Sessions->DestroySession(NAME_GameSession);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bLAN || IOnlineSubsystem::Get()->GetSubsystemName() == NULL_SUBSYSTEM;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUseLobbiesIfAvailable = true;

	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UTHSessionSubsystem::HandleCreateComplete));

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!LocalPlayer || !Sessions->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		OnHostComplete.Broadcast(false);
	}
}

void UTHSessionSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}

	OnHostComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful)
	{
		GetWorld()->ServerTravel(PendingHostMap + TEXT("?listen"));
	}
}

// Find

void UTHSessionSubsystem::FindSessions(bool bLAN, int32 MaxResults)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions.IsValid() || !LocalPlayer)
	{
		OnFindComplete.Broadcast({}, false);
		return;
	}

	LastSearch = MakeShared<FOnlineSessionSearch>();
	LastSearch->MaxSearchResults = MaxResults;
	LastSearch->bIsLanQuery = bLAN || IOnlineSubsystem::Get()->GetSubsystemName() == NULL_SUBSYSTEM;
	LastSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTHSessionSubsystem::HandleFindComplete));

	if (!Sessions->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		OnFindComplete.Broadcast({}, false);
	}
}

void UTHSessionSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	TArray<FTHSessionResult> Results;
	if (LastSearch.IsValid())
	{
		for (int32 Index = 0; Index < LastSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Found = LastSearch->SearchResults[Index];

			FTHSessionResult& Result = Results.AddDefaulted_GetRef();
			Result.HostName = Found.Session.OwningUserName;
			Result.MaxPlayers = Found.Session.SessionSettings.NumPublicConnections;
			Result.CurrentPlayers = Result.MaxPlayers - Found.Session.NumOpenPublicConnections;
			Result.PingMs = Found.PingInMs;
			Result.SearchIndex = Index;
		}
	}

	OnFindComplete.Broadcast(Results, bWasSuccessful);
}

// Join

void UTHSessionSubsystem::JoinSession(int32 SearchIndex)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions.IsValid() || !LocalPlayer || !LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTHSessionSubsystem::HandleJoinComplete));

	if (!Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, LastSearch->SearchResults[SearchIndex]))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		OnJoinComplete.Broadcast(false);
	}
}

void UTHSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	FString ConnectString;
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success
		&& Sessions.IsValid() && Sessions->GetResolvedConnectString(SessionName, ConnectString);

	OnJoinComplete.Broadcast(bSuccess);

	if (bSuccess)
	{
		if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
		{
			PC->ClientTravel(ConnectString, TRAVEL_Absolute);
		}
	}
}

// Leave

void UTHSessionSubsystem::LeaveSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	bHostAfterDestroy = false;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UTHSessionSubsystem::HandleDestroyComplete));
	Sessions->DestroySession(NAME_GameSession);
}

void UTHSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	if (bHostAfterDestroy)
	{
		bHostAfterDestroy = false;
		HostSession(PendingHostMap, PendingMaxPlayers, bPendingLAN);
	}
}

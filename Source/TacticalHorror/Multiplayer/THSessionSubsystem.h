#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "THSessionSubsystem.generated.h"

class FOnlineSessionSearch;

/** Blueprint-facing summary of a found session. */
USTRUCT(BlueprintType)
struct FTHSessionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 PingMs = 0;

	/** Pass back to JoinSession. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 SearchIndex = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTHSessionOperationComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTHFindSessionsComplete, const TArray<FTHSessionResult>&, Results, bool, bWasSuccessful);

/**
 * Hosting, finding and joining player-hosted (listen server) sessions.
 * Wraps whichever OnlineSubsystem is configured (Null for LAN now; EOS later), so nothing else in the game
 * talks to a session provider directly.
 */
UCLASS()
class TACTICALHORROR_API UTHSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Creates a session and travels to MapPath as a listen server. */
	UFUNCTION(BlueprintCallable, Category = "Session")
	void HostSession(const FString& MapPath, int32 MaxPlayers = 4, bool bLAN = true);

	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindSessions(bool bLAN = true, int32 MaxResults = 50);

	/** SearchIndex comes from the last FindSessions result. */
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinSession(int32 SearchIndex);

	UFUNCTION(BlueprintCallable, Category = "Session")
	void LeaveSession();

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FTHSessionOperationComplete OnHostComplete;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FTHFindSessionsComplete OnFindComplete;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FTHSessionOperationComplete OnJoinComplete;

private:
	IOnlineSessionPtr GetSessionInterface() const;

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);

	TSharedPtr<FOnlineSessionSearch> LastSearch;
	FString PendingHostMap;

	/** Set when HostSession had to destroy a stale session first. */
	int32 PendingMaxPlayers = 4;
	bool bPendingLAN = true;
	bool bHostAfterDestroy = false;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
};

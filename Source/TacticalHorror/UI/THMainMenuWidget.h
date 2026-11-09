#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Multiplayer/THSessionSubsystem.h"
#include "THMainMenuWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UTHAudioSettingsWidget;
class UTHSessionEntryWidget;

/**
 * Main menu: Host Game, Find Games (LAN list with Join per game), Settings, Quit. All session work goes through
 * UTHSessionSubsystem, so it works the same once the online subsystem is Steam instead of Null (LAN).
 *
 * Works on its own with a plain layout built in code (placeholder text, see AI_CONTENT.md). To design the real menu,
 * make a Widget Blueprint based on this class (e.g. /Game/UI/WBP_MainMenu) and give it widgets with these names, all
 * optional: HostButton, MapButton and MapText (picks the map to host), FindButton, SettingsButton, QuitButton,
 * SessionList (any panel, e.g. a Scroll Box or Vertical Box; found games are added to it) and StatusText. Then point ATHMenuPlayerController's Main Menu Widget Class at it
 * (DefaultGame.ini). The logic needs no changes.
 */
UCLASS(Config = Game)
class TACTICALHORROR_API UTHMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Map a hosted game starts on (a listen server), when HostMaps is empty. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Menu")
	FString HostMapPath = TEXT("/Game/Maps/L_FiringRange");

	/** Maps the host can pick from with the Map button, first is the default. Each must also be in MapsToCook. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Menu")
	TArray<FString> HostMaps;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (ClampMin = 1, ClampMax = 16))
	int32 MaxPlayers = 4;

	/** Row widget for each found game. Empty = UTHSessionEntryWidget's plain layout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<UTHSessionEntryWidget> SessionEntryClass;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Called whenever the status line changes ("Searching…", errors), for Blueprints that show it their own way. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu", meta = (DisplayName = "On Status Changed"))
	void ReceiveStatusChanged(const FText& Status);

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	/** Cycles through HostMaps. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> MapButton;

	/** Shows the picked map, e.g. "Map: Demo". */
	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapText;

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> FindButton;

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SessionList;

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	void BuildLayout();
	UButton* AddMenuButton(class UVerticalBox* Box, FName Name, const FText& Label);
	UTHSessionSubsystem* GetSessions() const;
	void SetStatus(const FText& Status);
	void SetBusy(bool bBusy);

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleMapClicked();

	/** The map Host Game starts. */
	FString GetHostMap() const;
	void UpdateMapText();

	UFUNCTION()
	void HandleFindClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleHostComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleFindComplete(const TArray<FTHSessionResult>& Results, bool bWasSuccessful);

	UFUNCTION()
	void HandleJoinComplete(bool bWasSuccessful);

	void JoinGame(int32 SearchIndex);
	void CloseSettings();

	/** th.AutoMenu: 1 = host, 2 = find and join the first game (retrying), for off-screen tests. */
	void RunAutoMenu();

	UPROPERTY(Transient)
	TObjectPtr<UTHAudioSettingsWidget> SettingsWidget;

	int32 MapIndex = 0;

	FTimerHandle AutoMenuTimer;
	int32 AutoFindAttempts = 0;
};

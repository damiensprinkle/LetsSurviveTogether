#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Multiplayer/THSessionSubsystem.h"
#include "THSessionEntryWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FTHOnSessionEntryJoin, int32 /*SearchIndex*/);

/**
 * One found game in the main menu's list: host name, players, ping and a Join button.
 *
 * Works on its own with a plain layout built in code (placeholder text, see AI_CONTENT.md). To restyle it, make a
 * Widget Blueprint based on this class with widgets named JoinButton, NameText and InfoText (all optional), and set
 * it as the main menu's Session Entry Class. ReceiveSetup lets the Blueprint show anything else from the result.
 */
UCLASS()
class TACTICALHORROR_API UTHSessionEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FTHSessionResult& InResult);

	FTHOnSessionEntryJoin OnJoin;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Session", meta = (DisplayName = "Setup"))
	void ReceiveSetup(const FTHSessionResult& Result);

	UPROPERTY(BlueprintReadOnly, Category = "Session", meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(BlueprintReadOnly, Category = "Session", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Session", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InfoText;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleJoinClicked();

	FTHSessionResult SessionResult;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "THPlayerController.generated.h"

class UTHAudioSettingsWidget;

/**
 * Owns player input and UI. Gameplay input is bound by the pawn and its components.
 */
UCLASS()
class TACTICALHORROR_API ATHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATHPlayerController();

	/** Escape or P. Escape doesn't reach the game in play-in-editor (it stops the session), so use P there. */
	UFUNCTION(Exec, BlueprintCallable, Category = "UI")
	void ToggleAudioSettings();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsAudioSettingsOpen() const;

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Replace with a Widget Blueprint subclass to restyle the menu. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTHAudioSettingsWidget> AudioSettingsWidgetClass;

private:
	void OpenAudioSettings();
	void CloseAudioSettings();

	UPROPERTY(Transient)
	TObjectPtr<UTHAudioSettingsWidget> AudioSettingsWidget;
};

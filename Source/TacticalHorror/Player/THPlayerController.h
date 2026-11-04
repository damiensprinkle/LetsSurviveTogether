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

	/**
	 * Settings panel's Leave Game (also a console command): leave the session and go back to the main menu.
	 * A host leaving ends the game for everyone; their clients go back to their menu.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "UI")
	void LeaveGame();

	/** Console: give this player the hand flashlight and take it out with the inspect, as picking one up does (for testing). */
	UFUNCTION(Exec)
	void GiveFlashlight();

	/** Console: switch the weapon light (as L). */
	UFUNCTION(Exec)
	void WeaponLight();

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Replace with a Widget Blueprint subclass to restyle the menu. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTHAudioSettingsWidget> AudioSettingsWidgetClass;

	/** Crosshair shown while holding a weapon (style / size / colour from the settings menu). */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UTHCrosshairWidget> CrosshairWidgetClass;

	/** Ammo readout shown while holding a weapon. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UTHAmmoWidget> AmmoWidgetClass;

private:
	void OpenAudioSettings();
	void CloseAudioSettings();

	UPROPERTY(Transient)
	TObjectPtr<class UTHAmmoWidget> AmmoWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTHAudioSettingsWidget> AudioSettingsWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UTHCrosshairWidget> CrosshairWidget;
};

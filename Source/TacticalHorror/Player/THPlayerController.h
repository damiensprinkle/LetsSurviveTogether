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

	/** Console: set your sanity, 0-100 (for testing; th.SanityDebug 1 shows it). Hallucinate <name>: hear one now (none: any). */
	UFUNCTION(Exec)
	void Sanity(float Value);

	UFUNCTION(Exec)
	void Hallucinate(const FString& Name);

	UFUNCTION(Server, Reliable)
	void ServerSetSanity(float Value);

	/** Look input, scaled by the player's look sensitivity (and aim sensitivity while aiming down sights). */
	virtual void AddYawInput(float Val) override;
	virtual void AddPitchInput(float Val) override;

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

	/** Full-screen status while downed, dead, or when the mission has failed. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UTHStatusWidget> StatusWidgetClass;

public:
	class UTHStatusWidget* GetStatusWidget() const { return StatusWidget; }

	/** The local player was hurt (Damage points): jolt the view and splash blood on the screen. */
	void NotifyLocalHit(float Damage);

	class UTHCameraModifier* GetBodyCameraModifier() const { return BodyCameraModifier; }

private:
	float GetLookScale() const;
	void OpenAudioSettings();
	void CloseAudioSettings();

	UPROPERTY(Transient)
	TObjectPtr<class UTHAmmoWidget> AmmoWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTHAudioSettingsWidget> AudioSettingsWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UTHCrosshairWidget> CrosshairWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UTHStatusWidget> StatusWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UTHCameraModifier> BodyCameraModifier;
};

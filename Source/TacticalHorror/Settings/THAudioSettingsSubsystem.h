#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "THAudioSettingsSubsystem.generated.h"

/**
 * Player volume settings, saved per user in GameUserSettings.ini. Local only: each player hears the game their way.
 *
 * Master scales everything through the audio device. Gunshots scale the weapon fire sounds where they are spawned
 * (ATHWeaponBase). A new category is a new property here plus a multiplier where that sound is played.
 */
UCLASS(Config = GameUserSettings)
class TACTICALHORROR_API UTHAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Audio Settings")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintPure, Category = "Audio Settings")
	float GetGunshotVolume() const { return GunshotVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void SetGunshotVolume(float Volume);

	/** Writes the current values to GameUserSettings.ini. */
	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void SaveSettings();

	/** Gunshot volume for whoever is listening on this machine; 1 when there is no game instance (editor previews). */
	static float GetGunshotVolumeFor(const UObject* WorldContext);

private:
	void ApplyMasterVolume(float Volume) const;

	UPROPERTY(Config)
	float MasterVolume = 1.0f;

	UPROPERTY(Config)
	float GunshotVolume = 1.0f;
};

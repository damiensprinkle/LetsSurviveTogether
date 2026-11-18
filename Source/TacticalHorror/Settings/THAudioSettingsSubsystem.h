#pragma once

#include "CoreMinimal.h"
#include "AudioDeviceHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "THAudioSettingsSubsystem.generated.h"

/**
 * Player volume settings, saved per user in GameUserSettings.ini. Local only: each player hears the game their way.
 *
 * Master scales everything through the audio device. Music scales the background music and ambience. Gunplay scales every gun sound where it is played: shots, reload
 * and draw foley (THAnimNotify_WeaponSound), shell casings, dropped magazines, dry fire and bullet impacts. A new
 * category is a new property here plus a multiplier where that sound is played.
 *
 * 3D audio (on by default) turns on binaural (HRTF) rendering from the Steam Audio plugin for the sounds whose attenuation
 * asks for it (SA_EnemyVoice, SA_EnemyFootsteps): an enemy above, below or behind sounds like it, on headphones. Off falls
 * back to plain left/right panning, which can suit speakers better.
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
	float GetGunplayVolume() const { return GunplayVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void SetGunplayVolume(float Volume);

	/** Music and ambience (UTHAmbienceSubsystem): background music and drones. */
	UFUNCTION(BlueprintPure, Category = "Audio Settings")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void SetMusicVolume(float Volume);

	/** Binaural (HRTF) rendering for enemy sounds; see the class comment. */
	UFUNCTION(BlueprintPure, Category = "Audio Settings")
	bool Is3DAudioEnabled() const { return b3DAudio; }

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void Set3DAudioEnabled(bool bEnabled);

	/** Writes the current values to GameUserSettings.ini. */
	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void SaveSettings();

	/** Gunplay volume for whoever is listening on this machine; 1 when there is no game instance (editor previews). */
	static float GetGunplayVolumeFor(const UObject* WorldContext);

	/** Music and ambience volume on this machine; 1 when there is no game instance. */
	static float GetMusicVolumeFor(const UObject* WorldContext);

private:
	void ApplyMasterVolume(float Volume) const;
	void Apply3DAudio(bool bEnabled) const;
	FAudioDeviceHandle GetAudioDevice() const;

	UPROPERTY(Config)
	float MasterVolume = 1.0f;

	UPROPERTY(Config)
	float GunplayVolume = 1.0f;

	UPROPERTY(Config)
	float MusicVolume = 1.0f;

	UPROPERTY(Config)
	bool b3DAudio = true;
};

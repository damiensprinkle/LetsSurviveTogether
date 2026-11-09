#include "Settings/THAudioSettingsSubsystem.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

void UTHAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The Gunplay slider was called Gunshots: keep a player's old setting
	float OldGunshotVolume = 1.0f;
	float Ignored = 0.0f;
	const FString Section = GetClass()->GetPathName();
	if (!GConfig->GetFloat(*Section, TEXT("GunplayVolume"), Ignored, GGameUserSettingsIni)
		&& GConfig->GetFloat(*Section, TEXT("GunshotVolume"), OldGunshotVolume, GGameUserSettingsIni))
	{
		GunplayVolume = OldGunshotVolume;
	}
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	GunplayVolume = FMath::Clamp(GunplayVolume, 0.0f, 1.0f);
	ApplyMasterVolume(MasterVolume);
}

void UTHAudioSettingsSubsystem::Deinitialize()
{
	SaveSettings();
	// The editor shares the audio device with play-in-editor, so don't leave it turned down after the session
	ApplyMasterVolume(1.0f);
	Super::Deinitialize();
}

void UTHAudioSettingsSubsystem::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyMasterVolume(MasterVolume);
}

void UTHAudioSettingsSubsystem::SetGunplayVolume(float Volume)
{
	GunplayVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UTHAudioSettingsSubsystem::SaveSettings()
{
	SaveConfig();
}

float UTHAudioSettingsSubsystem::GetGunplayVolumeFor(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UTHAudioSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UTHAudioSettingsSubsystem>() : nullptr;
	return Settings ? Settings->GunplayVolume : 1.0f;
}

void UTHAudioSettingsSubsystem::ApplyMasterVolume(float Volume) const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
	if (!Device && GEngine)
	{
		Device = GEngine->GetMainAudioDevice();
	}
	if (Device)
	{
		Device->SetTransientPrimaryVolume(Volume);
	}
}

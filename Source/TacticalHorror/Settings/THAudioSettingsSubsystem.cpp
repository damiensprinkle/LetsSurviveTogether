#include "Settings/THAudioSettingsSubsystem.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UTHAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	GunshotVolume = FMath::Clamp(GunshotVolume, 0.0f, 1.0f);
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

void UTHAudioSettingsSubsystem::SetGunshotVolume(float Volume)
{
	GunshotVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UTHAudioSettingsSubsystem::SaveSettings()
{
	SaveConfig();
}

float UTHAudioSettingsSubsystem::GetGunshotVolumeFor(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UTHAudioSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UTHAudioSettingsSubsystem>() : nullptr;
	return Settings ? Settings->GunshotVolume : 1.0f;
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

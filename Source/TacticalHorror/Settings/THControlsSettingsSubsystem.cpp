#include "Settings/THControlsSettingsSubsystem.h"

void UTHControlsSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LookSensitivity = FMath::Clamp(LookSensitivity, MinSensitivity, MaxSensitivity);
	AimSensitivity = FMath::Clamp(AimSensitivity, MinSensitivity, MaxSensitivity);
}

void UTHControlsSettingsSubsystem::Deinitialize()
{
	SaveSettings();
	Super::Deinitialize();
}

void UTHControlsSettingsSubsystem::SetLookSensitivity(float Value)
{
	LookSensitivity = FMath::Clamp(Value, MinSensitivity, MaxSensitivity);
}

void UTHControlsSettingsSubsystem::SetAimSensitivity(float Value)
{
	AimSensitivity = FMath::Clamp(Value, MinSensitivity, MaxSensitivity);
}

void UTHControlsSettingsSubsystem::SaveSettings()
{
	SaveConfig();
}

#define LOCTEXT_NAMESPACE "THControlsSettings"

const TArray<FTHOtherGameSensitivity>& UTHControlsSettingsSubsystem::GetOtherGames()
{
	// Degrees per mouse count at sensitivity 1, as published for each game (hip fire, default settings, raw input).
	// Source-engine games share 0.022; Valorant is 0.07; Overwatch and Call of Duty since Modern Warfare (2019) are 0.0066.
	static const TArray<FTHOtherGameSensitivity> Games = {
		{ LOCTEXT("CS2", "Counter-Strike 2 / CS:GO"), 0.022 },
		{ LOCTEXT("Valorant", "Valorant"), 0.07 },
		{ LOCTEXT("Apex", "Apex Legends"), 0.022 },
		{ LOCTEXT("Overwatch", "Overwatch 2"), 0.0066 },
		{ LOCTEXT("CallOfDuty", "Call of Duty (MW 2019 and later)"), 0.0066 },
		{ LOCTEXT("TF2", "Team Fortress 2"), 0.022 },
		{ LOCTEXT("Titanfall", "Titanfall 2"), 0.022 },
		{ LOCTEXT("Quake", "Quake Champions"), 0.022 },
	};
	return Games;
}

#undef LOCTEXT_NAMESPACE

float UTHControlsSettingsSubsystem::ConvertFrom(int32 GameIndex, float TheirSensitivity)
{
	const TArray<FTHOtherGameSensitivity>& Games = GetOtherGames();
	if (!Games.IsValidIndex(GameIndex))
	{
		return 0.0f;
	}
	return static_cast<float>(TheirSensitivity * Games[GameIndex].DegreesPerCount / DegreesPerCount);
}

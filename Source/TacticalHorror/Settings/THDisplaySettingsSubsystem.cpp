#include "Settings/THDisplaySettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UTHDisplaySettingsSubsystem::Deinitialize()
{
	SaveSettings();
	Super::Deinitialize();
}

void UTHDisplaySettingsSubsystem::SaveSettings()
{
	SaveConfig();
}

bool UTHDisplaySettingsSubsystem::IsBloodEnabledFor(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UTHDisplaySettingsSubsystem* Display = GameInstance ? GameInstance->GetSubsystem<UTHDisplaySettingsSubsystem>() : nullptr;
	return !Display || Display->IsBloodEnabled();
}

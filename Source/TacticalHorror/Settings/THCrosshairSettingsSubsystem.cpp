#include "Settings/THCrosshairSettingsSubsystem.h"
#include "UI/THCrosshairWidget.h"
#include "Engine/Texture2D.h"

void UTHCrosshairSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Size = FMath::Clamp(Size, MinSize, MaxSize);
	Color.A = 255;
}

void UTHCrosshairSettingsSubsystem::Deinitialize()
{
	SaveSettings();
	Super::Deinitialize();
}

void UTHCrosshairSettingsSubsystem::SetStyle(int32 NewStyle)
{
	const int32 Count = GetStyles().Num();
	Style = Count > 0 ? ((NewStyle % Count) + Count) % Count : 0;
	OnChanged.Broadcast();
}

void UTHCrosshairSettingsSubsystem::SetSize(float NewSize)
{
	Size = FMath::Clamp(NewSize, MinSize, MaxSize);
	OnChanged.Broadcast();
}

void UTHCrosshairSettingsSubsystem::SetColor(FColor NewColor)
{
	Color = NewColor;
	Color.A = 255;
	OnChanged.Broadcast();
}

const TArray<TSoftObjectPtr<UTexture2D>>& UTHCrosshairSettingsSubsystem::GetStyles()
{
	return GetDefault<UTHCrosshairWidget>()->Styles;
}

UTexture2D* UTHCrosshairSettingsSubsystem::GetStyleTexture(int32 StyleIndex) const
{
	const TArray<TSoftObjectPtr<UTexture2D>>& Styles = GetStyles();
	return Styles.IsValidIndex(StyleIndex) ? Styles[StyleIndex].LoadSynchronous() : nullptr;
}

void UTHCrosshairSettingsSubsystem::SaveSettings()
{
	SaveConfig();
}

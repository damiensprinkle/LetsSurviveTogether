#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "THCrosshairSettingsSubsystem.generated.h"

class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FTHOnCrosshairSettingsChanged);

/**
 * The player's crosshair choice: style (which image), size and colour. Saved per user in GameUserSettings.ini;
 * local only. The list of styles is project config (UTHCrosshairWidget::Styles in DefaultGame.ini).
 */
UCLASS(Config = GameUserSettings)
class TACTICALHORROR_API UTHCrosshairSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float MinSize = 8.0f;
	static constexpr float MaxSize = 128.0f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Index into the style list (wraps). */
	UFUNCTION(BlueprintPure, Category = "Crosshair Settings")
	int32 GetStyle() const { return Style; }

	UFUNCTION(BlueprintCallable, Category = "Crosshair Settings")
	void SetStyle(int32 NewStyle);

	/** On-screen size in pixels (at 1080p UI scale). */
	UFUNCTION(BlueprintPure, Category = "Crosshair Settings")
	float GetSize() const { return Size; }

	UFUNCTION(BlueprintCallable, Category = "Crosshair Settings")
	void SetSize(float NewSize);

	UFUNCTION(BlueprintPure, Category = "Crosshair Settings")
	FColor GetColor() const { return Color; }

	/** Opacity is ignored: colour only. */
	UFUNCTION(BlueprintCallable, Category = "Crosshair Settings")
	void SetColor(FColor NewColor);

	/** The style list from project config, and the chosen style's image (null if the list is empty). */
	static const TArray<TSoftObjectPtr<UTexture2D>>& GetStyles();
	UTexture2D* GetStyleTexture(int32 StyleIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Crosshair Settings")
	void SaveSettings();

	FTHOnCrosshairSettingsChanged OnChanged;

private:
	UPROPERTY(Config)
	int32 Style = 0;

	UPROPERTY(Config)
	float Size = 40.0f;

	UPROPERTY(Config)
	FColor Color = FColor::White;
};

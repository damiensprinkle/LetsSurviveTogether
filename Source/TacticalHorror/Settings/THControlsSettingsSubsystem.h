#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "THControlsSettingsSubsystem.generated.h"

/** Another game's mouse sensitivity scale, for the converter: degrees turned per mouse count at sensitivity 1. */
struct FTHOtherGameSensitivity
{
	FText Name;
	double DegreesPerCount;
};

/**
 * Player control settings, saved per user in GameUserSettings.ini; local only.
 *
 * Look sensitivity scales all turning (mouse and stick). Aim sensitivity is a further multiplier while aiming down
 * sights, blended in with the aim (UTHWeaponComponent::GetAimAlpha). Both are applied in ATHPlayerController's
 * AddYawInput / AddPitchInput, so whatever drives the look input (the pawn's Enhanced Input bindings) is scaled.
 *
 * The converter turns another game's sensitivity into ours so a mouse movement turns the same distance (same mouse,
 * same DPI). Hip fire only: games treat aiming down sights too differently to convert.
 */
UCLASS(Config = GameUserSettings)
class TACTICALHORROR_API UTHControlsSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float MinSensitivity = 0.02f;
	static constexpr float MaxSensitivity = 3.0f;

	/**
	 * Degrees our view turns per mouse count at look sensitivity 1: the mouse axis sensitivity 0.07 (DefaultInput.ini
	 * AxisConfig) times the engine's legacy yaw / pitch scale 2.5 (bEnableLegacyInputScales). Measured by the
	 * Functional.MouseTurn test; change it if either setting changes.
	 */
	static constexpr double DegreesPerCount = 0.175;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Controls Settings")
	float GetLookSensitivity() const { return LookSensitivity; }

	UFUNCTION(BlueprintCallable, Category = "Controls Settings")
	void SetLookSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "Controls Settings")
	float GetAimSensitivity() const { return AimSensitivity; }

	UFUNCTION(BlueprintCallable, Category = "Controls Settings")
	void SetAimSensitivity(float Value);

	UFUNCTION(BlueprintCallable, Category = "Controls Settings")
	void SaveSettings();

	/** Games the converter knows. */
	static const TArray<FTHOtherGameSensitivity>& GetOtherGames();

	/** Our look sensitivity matching TheirSensitivity in GetOtherGames()[GameIndex] (not clamped to our range); 0 for a bad index. */
	static float ConvertFrom(int32 GameIndex, float TheirSensitivity);

private:
	UPROPERTY(Config)
	float LookSensitivity = 1.0f;

	UPROPERTY(Config)
	float AimSensitivity = 1.0f;
};

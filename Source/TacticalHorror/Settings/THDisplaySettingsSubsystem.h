#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "THDisplaySettingsSubsystem.generated.h"

/**
 * What the player sees, saved per user in GameUserSettings.ini; local only (each player picks for their own screen).
 *
 * Blood (developer, 2026-11-17: on / off): off, shots on flesh show no blood spray or splatter on the walls, a limb
 * coming off sprays none, and being hit puts no blood on the screen. Sounds, the hit jolt and the limbs coming off
 * stay. Blood already on the walls stays until it fades.
 */
UCLASS(Config = GameUserSettings)
class TACTICALHORROR_API UTHDisplaySettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Display Settings")
	bool IsBloodEnabled() const { return bBlood; }

	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	void SetBloodEnabled(bool bEnabled) { bBlood = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	void SaveSettings();

	/** The Blood setting of the game WorldContext is in; on when there is none (a dedicated server shows nothing anyway). */
	static bool IsBloodEnabledFor(const UObject* WorldContext);

private:
	UPROPERTY(Config)
	bool bBlood = true;
};

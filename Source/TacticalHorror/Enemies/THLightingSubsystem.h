#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "THLightingSubsystem.generated.h"

class APawn;
class ULightComponent;

/**
 * How lit a player is, for enemies' eyes (developer, 2026-11-13: light and dark in what enemies see). 1 with a light of
 * their own on (a flashlight, a weapon light); else the brightest lamp on them: a point, spot or rect light in the level
 * (not one someone carries) whose reach they are within, with a clear line from it to them, 1 at the lamp fading to 0
 * at its reach (and only inside a spot light's cone). Server; the lamps are listed every few seconds and each player's
 * answer kept a quarter of a second.
 */
UCLASS()
class TACTICALHORROR_API UTHLightingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** At this or above a player counts as lit. */
	static constexpr float LitAbove = 0.3f;

	float GetLightOn(const APawn* Player) const;

private:
	float Measure(const APawn* Player) const;

	mutable TArray<TWeakObjectPtr<ULightComponent>> Lamps;
	mutable double LampsListedAt = -1.0e9;
	mutable TMap<TWeakObjectPtr<const APawn>, TPair<double, float>> Cache;
};

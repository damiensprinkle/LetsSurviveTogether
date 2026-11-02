#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "THGameMode.generated.h"

/**
 * Server-only mission rules: spawning, friendly fire, and the team-wipe restart.
 */
UCLASS()
class TACTICALHORROR_API ATHGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATHGameMode();

	/** Called by health components when a player is downed or killed. */
	virtual void NotifyPlayerIncapacitated(AController* Victim);

	/** Restarts the current map for everyone (seamless travel keeps the session). */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	virtual void RestartMission();

	bool IsFriendlyFireEnabled() const { return bFriendlyFire; }

protected:
	/** Whether player weapons can damage other players. Undecided in the GDD; off by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	bool bFriendlyFire = false;

	/** Delay between a team wipe and the mission restarting, so players see what happened. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = 0))
	float TeamWipeRestartDelay = 5.0f;

	FTimerHandle RestartTimer;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "THPlayerState.generated.h"

/**
 * Per-player replicated stats. Feeds the mission debrief.
 */
UCLASS()
class TACTICALHORROR_API ATHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	/** Clears the per-mission stats. Server only. */
	void ResetMissionStats();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 Kills = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 Deaths = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 TimesDowned = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 Revives = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "THGameState.generated.h"

/** Coarse phases of the mission lifecycle (GDD section 11). */
UENUM(BlueprintType)
enum class ETHMissionPhase : uint8
{
	Preparation,
	Briefing,
	Deployment,
	Active,
	Extraction,
	Debrief,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTHMissionPhaseChanged, ETHMissionPhase, NewPhase, ETHMissionPhase, OldPhase);

/**
 * Replicated, shared mission state. The server (GameMode) decides; this class tells every client.
 */
UCLASS()
class TACTICALHORROR_API ATHGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Mission")
	ETHMissionPhase GetMissionPhase() const { return MissionPhase; }

	/** Server only. */
	void SetMissionPhase(ETHMissionPhase NewPhase);

	/** True when every player with a pawn is downed or dead. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	bool AreAllPlayersIncapacitated() const;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FTHMissionPhaseChanged OnMissionPhaseChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_MissionPhase, BlueprintReadOnly, Category = "Mission")
	ETHMissionPhase MissionPhase = ETHMissionPhase::Active;

	UFUNCTION()
	void OnRep_MissionPhase(ETHMissionPhase OldPhase);
};

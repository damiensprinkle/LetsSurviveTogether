#include "Core/THGameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Player/Components/THHealthComponent.h"
#include "Net/UnrealNetwork.h"

void ATHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATHGameState, MissionPhase);
}

void ATHGameState::SetMissionPhase(ETHMissionPhase NewPhase)
{
	if (!HasAuthority() || NewPhase == MissionPhase)
	{
		return;
	}

	const ETHMissionPhase OldPhase = MissionPhase;
	MissionPhase = NewPhase;

	// RepNotifies don't run on the server, so notify the listen-server host directly
	OnRep_MissionPhase(OldPhase);
}

bool ATHGameState::AreAllPlayersIncapacitated() const
{
	int32 NumPlayersWithPawns = 0;

	for (const APlayerState* PS : PlayerArray)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		++NumPlayersWithPawns;

		const UTHHealthComponent* Health = Pawn->FindComponentByClass<UTHHealthComponent>();
		if (!Health || Health->IsAlive())
		{
			return false;
		}
	}

	return NumPlayersWithPawns > 0;
}

void ATHGameState::OnRep_MissionPhase(ETHMissionPhase OldPhase)
{
	OnMissionPhaseChanged.Broadcast(MissionPhase, OldPhase);
}

#include "Core/THPlayerState.h"
#include "Net/UnrealNetwork.h"

void ATHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATHPlayerState, Kills);
	DOREPLIFETIME(ATHPlayerState, Deaths);
	DOREPLIFETIME(ATHPlayerState, TimesDowned);
	DOREPLIFETIME(ATHPlayerState, Revives);
}

void ATHPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// Carried across seamless travel so the debrief can still read them in the staging area
	if (ATHPlayerState* Other = Cast<ATHPlayerState>(PlayerState))
	{
		Other->Kills = Kills;
		Other->Deaths = Deaths;
		Other->TimesDowned = TimesDowned;
		Other->Revives = Revives;
	}
}

void ATHPlayerState::ResetMissionStats()
{
	Kills = 0;
	Deaths = 0;
	TimesDowned = 0;
	Revives = 0;
}

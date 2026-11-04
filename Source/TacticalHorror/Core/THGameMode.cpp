#include "Core/THGameMode.h"
#include "Core/THGameState.h"
#include "Core/THPlayerState.h"
#include "Player/THPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/DefaultPawn.h"
#include "TacticalHorror.h"

ATHGameMode::ATHGameMode()
{
	GameStateClass = ATHGameState::StaticClass();
	PlayerStateClass = ATHPlayerState::StaticClass();
	PlayerControllerClass = ATHPlayerController::StaticClass();

	// The horror template's character Blueprint holds the interaction, flashlight and night vision logic.
	// Reparent it to ATHCharacter in the editor to give it health and weapons.
	TemplatePawnClass = TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Game/Blueprint/Character/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")));

	bUseSeamlessTravel = true;
}

void ATHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// A Blueprint game mode that picks its own pawn keeps it
	if ((!DefaultPawnClass || DefaultPawnClass == ADefaultPawn::StaticClass()) && !TemplatePawnClass.IsNull())
	{
		if (UClass* PawnClass = TemplatePawnClass.LoadSynchronous())
		{
			DefaultPawnClass = PawnClass;
		}
	}
	Super::InitGame(MapName, Options, ErrorMessage);
}

void ATHGameMode::NotifyPlayerIncapacitated(AController* Victim)
{
	ATHGameState* THGameState = GetGameState<ATHGameState>();
	if (!THGameState || RestartTimer.IsValid() || !THGameState->AreAllPlayersIncapacitated())
	{
		return;
	}

	UE_LOG(LogTacticalHorror, Log, TEXT("Team wipe. Restarting mission in %.1fs"), TeamWipeRestartDelay);
	THGameState->SetMissionPhase(ETHMissionPhase::Failed);
	GetWorldTimerManager().SetTimer(RestartTimer, this, &ATHGameMode::RestartMission, FMath::Max(TeamWipeRestartDelay, 0.01f), false);
}

void ATHGameMode::RestartMission()
{
	GetWorldTimerManager().ClearTimer(RestartTimer);

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ATHPlayerState* THPS = Cast<ATHPlayerState>(PS))
		{
			THPS->ResetMissionStats();
		}
	}

	GetWorld()->ServerTravel(TEXT("?restart"));
}

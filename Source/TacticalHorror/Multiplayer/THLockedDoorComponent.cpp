#include "Multiplayer/THLockedDoorComponent.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Multiplayer/THKeyPickupComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/THCharacter.h"
#include "Sound/SoundBase.h"

void UTHLockedDoorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTHLockedDoorComponent, bUnlocked);
	DOREPLIFETIME(UTHLockedDoorComponent, bOpen);
}

void UTHLockedDoorComponent::BeginPlay()
{
	Super::BeginPlay();
	DisableBlueprintOverlap(TEXT("Box"));
	DoorTimeline = FindOwnerComponent<UTimelineComponent>(TEXT("DoorTimeline"));
	RattleTimeline = FindOwnerComponent<UTimelineComponent>(TEXT("DoorLockMove"));
}

void UTHLockedDoorComponent::ServerPressed(ATHCharacter* By)
{
	if (!bUnlocked)
	{
		if (!UTHKeyPickupComponent::PlayerHasKey(By))
		{
			MulticastRattle();
			return;
		}
		UTHKeyPickupComponent::UseKey(By);
		bUnlocked = true;
	}
	bOpen = !bOpen;
	ApplyOpenState();
}

void UTHLockedDoorComponent::OnRep_Open()
{
	ApplyOpenState();
}

void UTHLockedDoorComponent::ApplyOpenState()
{
	if (!DoorTimeline)
	{
		return;
	}
	AActor* Door = GetOwner();

	// The timeline's Finished event refreshes the local player's button prompt through this variable, which only the
	// Blueprint's overlap (now off) would have set
	if (const FObjectPropertyBase* PlayerVariable = FindFProperty<FObjectPropertyBase>(Door->GetClass(), TEXT("Player")))
	{
		const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
		APawn* LocalPawn = Controller ? Controller->GetPawn() : nullptr;
		if (LocalPawn && LocalPawn->IsA(PlayerVariable->PropertyClass))
		{
			PlayerVariable->SetObjectPropertyValue_InContainer(Door, LocalPawn);
		}
	}
	// Decides whether that Finished event plays the closing thud
	if (const FBoolProperty* IsClosed = FindFProperty<FBoolProperty>(Door->GetClass(), TEXT("IsClosed?")))
	{
		IsClosed->SetPropertyValue_InContainer(Door, !bOpen);
	}

	if (bOpen)
	{
		if (!bPlayedUnlock)
		{
			bPlayedUnlock = true;
			PlayDoorSound(UnlockSound);
			// The door's "locked" sign
			if (UTextRenderComponent* Sign = FindOwnerComponent<UTextRenderComponent>(TEXT("TextRender")))
			{
				Sign->SetVisibility(false);
			}
		}
		PlayDoorSound(OpenSound);
		DoorTimeline->Play();
	}
	else
	{
		PlayDoorSound(CloseSound);
		DoorTimeline->Reverse();
	}
}

void UTHLockedDoorComponent::MulticastRattle_Implementation()
{
	PlayDoorSound(LockedSound);
	if (RattleTimeline)
	{
		if (bRattleForward)
		{
			RattleTimeline->Play();
		}
		else
		{
			RattleTimeline->Reverse();
		}
		bRattleForward = !bRattleForward;
	}
}

void UTHLockedDoorComponent::PlayDoorSound(USoundBase* Sound) const
{
	if (!Sound)
	{
		return;
	}
	const USceneComponent* Panel = FindOwnerComponent<USceneComponent>(TEXT("Door"));
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Panel ? Panel->GetComponentLocation() : GetOwner()->GetActorLocation());
}

#include "Multiplayer/THKeyPickupComponent.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/THCharacter.h"
#include "Sound/SoundBase.h"

// The template character's variable
static const TCHAR* HasKeyVariable = TEXT("Has Key?");

void UTHKeyPickupComponent::BeginPlay()
{
	Super::BeginPlay();
	// The sphere that opens BP_Key's own input path. Its Inner and Sphere1 only show the pickup icon, so they stay.
	DisableBlueprintOverlap(TEXT("Sphere"));
	// A hidden actor with no collision isn't sent to clients, so someone joining while the key is taken would still see it
	GetOwner()->bAlwaysRelevant = true;
}

void UTHKeyPickupComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTHKeyPickupComponent, bTaken);
}

bool UTHKeyPickupComponent::PlayerHasKey(const AActor* Player)
{
	const FBoolProperty* HasKey = Player ? FindFProperty<FBoolProperty>(Player->GetClass(), HasKeyVariable) : nullptr;
	return HasKey && HasKey->GetPropertyValue_InContainer(Player);
}

void UTHKeyPickupComponent::ServerPressed(ATHCharacter* By)
{
	const FBoolProperty* HasKey = FindFProperty<FBoolProperty>(By->GetClass(), HasKeyVariable);
	// Has Key? can't count, so a second key would be lost when the first is used: it stays for another player
	if (HasKey && !bTaken && !HasKey->GetPropertyValue_InContainer(By))
	{
		HasKey->SetPropertyValue_InContainer(By, true);
		MulticastPlayPickupSound();
		Holder = By;
		By->OnEndPlay.AddDynamic(this, &UTHKeyPickupComponent::HandleHolderEndPlay);
		SetTaken(true);
	}
}

void UTHKeyPickupComponent::UseKey(AActor* Player)
{
	if (const FBoolProperty* HasKey = FindFProperty<FBoolProperty>(Player->GetClass(), HasKeyVariable))
	{
		HasKey->SetPropertyValue_InContainer(Player, false);
	}
	for (TActorIterator<AActor> It(Player->GetWorld()); It; ++It)
	{
		UTHKeyPickupComponent* Key = It->FindComponentByClass<UTHKeyPickupComponent>();
		if (Key && Key->Holder == Player)
		{
			Player->OnEndPlay.RemoveDynamic(Key, &UTHKeyPickupComponent::HandleHolderEndPlay);
			It->Destroy();
			return;
		}
	}
}

void UTHKeyPickupComponent::HandleHolderEndPlay(AActor* LeavingHolder, EEndPlayReason::Type Reason)
{
	// Destroyed is a player leaving. The others are the whole level closing, when there's nobody to give it back to.
	if (Reason == EEndPlayReason::Destroyed)
	{
		Holder = nullptr;
		SetTaken(false);
	}
}

void UTHKeyPickupComponent::SetTaken(bool bNewTaken)
{
	bTaken = bNewTaken;
	OnRep_Taken();
}

void UTHKeyPickupComponent::OnRep_Taken()
{
	// Collision off also takes it out of the interact trace and ends the overlaps showing its pickup icon
	GetOwner()->SetActorHiddenInGame(bTaken);
	GetOwner()->SetActorEnableCollision(!bTaken);
}

void UTHKeyPickupComponent::MulticastPlayPickupSound_Implementation()
{
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetOwner()->GetActorLocation());
	}
}

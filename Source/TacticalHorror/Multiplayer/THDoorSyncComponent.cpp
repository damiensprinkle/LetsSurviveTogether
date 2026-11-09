#include "Multiplayer/THDoorSyncComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/THCharacter.h"

UTHDoorSyncComponent::UTHDoorSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	SetIsReplicatedByDefault(true);
}

void UTHDoorSyncComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTHDoorSyncComponent, bServerOpen);
	DOREPLIFETIME(UTHDoorSyncComponent, bServerUnlocked);
}

void UTHDoorSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	const UClass* DoorClass = GetOwner()->GetClass();
	OpenProperty = FindFProperty<FBoolProperty>(DoorClass, TEXT("Opening"));
	if (!OpenProperty)
	{
		OpenProperty = FindFProperty<FBoolProperty>(DoorClass, TEXT("IsClosed"));
		bOpenPropertyInverted = OpenProperty != nullptr;
	}
	MovingProperty = FindFProperty<FBoolProperty>(DoorClass, TEXT("Is Door Moving?"));
	UnlockProperty = FindFProperty<FBoolProperty>(DoorClass, TEXT("CodeCorrect?"));
	InteractFunction = GetOwner()->FindFunction(TEXT("Interact"));

	if (!OpenProperty || !InteractFunction)
	{
		UE_LOG(LogTemp, Warning, TEXT("THDoorSync: %s has no Opening / IsClosed variable or no Interact event, so it will not sync."), *GetOwner()->GetName());
		SetComponentTickEnabled(false);
		return;
	}

	bLastSeenOpen = IsDoorOpen();
	if (GetOwner()->HasAuthority())
	{
		bServerOpen = bLastSeenOpen;
	}
}

bool UTHDoorSyncComponent::IsDoorOpen() const
{
	return OpenProperty && OpenProperty->GetPropertyValue_InContainer(GetOwner()) != bOpenPropertyInverted;
}

bool UTHDoorSyncComponent::IsDoorMoving() const
{
	return MovingProperty && MovingProperty->GetPropertyValue_InContainer(GetOwner());
}

APawn* UTHDoorSyncComponent::GetLocalPawn() const
{
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	return Controller ? Controller->GetPawn() : nullptr;
}

void UTHDoorSyncComponent::RunDoorInteract(APawn* Player)
{
	TArray<uint8> Arguments;
	Arguments.SetNumZeroed(InteractFunction->ParmsSize);
	InteractFunction->InitializeStruct(Arguments.GetData());
	for (TFieldIterator<FObjectProperty> It(InteractFunction); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		if (Player && Player->IsA(It->PropertyClass))
		{
			It->SetObjectPropertyValue_InContainer(Arguments.GetData(), Player);
		}
	}
	GetOwner()->ProcessEvent(InteractFunction, Arguments.GetData());
	InteractFunction->DestroyStruct(Arguments.GetData());
}

void UTHDoorSyncComponent::HandleRequest(APawn* InRequester, bool bOpen)
{
	if (!GetOwner()->HasAuthority() || !InRequester || IsLocked()
		|| FVector::Dist(InRequester->GetActorLocation(), GetOwner()->GetActorLocation()) > MaxRequestDistance)
	{
		return;
	}
	bHasRequest = true;
	bRequestedOpen = bOpen;
	Requester = InRequester;
}

void UTHDoorSyncComponent::HandleUnlockRequest(APawn* InRequester)
{
	if (GetOwner()->HasAuthority() && UnlockProperty && InRequester
		&& FVector::Dist(InRequester->GetActorLocation(), GetOwner()->GetActorLocation()) <= MaxRequestDistance)
	{
		bServerUnlocked = true;
	}
}

void UTHDoorSyncComponent::SyncUnlock()
{
	if (!UnlockProperty)
	{
		return;
	}
	AActor* Door = GetOwner();
	const bool bLocalCorrect = UnlockProperty->GetPropertyValue_InContainer(Door);
	if (Door->HasAuthority())
	{
		// The host entered it
		bServerUnlocked = bServerUnlocked || bLocalCorrect;
	}
	else if (bLocalCorrect && !bServerUnlocked && !bUnlockSent)
	{
		if (ATHCharacter* Character = Cast<ATHCharacter>(GetLocalPawn()))
		{
			bUnlockSent = true;
			Character->ServerRequestDoorUnlock(this);
		}
	}
	if (bServerUnlocked && !bLocalCorrect)
	{
		UnlockProperty->SetPropertyValue_InContainer(Door, true);
	}
}

void UTHDoorSyncComponent::TickServer(bool bOpen)
{
	if (bHasRequest && !IsDoorMoving())
	{
		bHasRequest = false;
		if (bOpen != bRequestedOpen)
		{
			RunDoorInteract(Requester.IsValid() ? Requester.Get() : GetLocalPawn());
			bOpen = IsDoorOpen();
		}
	}
	// Whatever moved it (the request above, or the host's own press), this is now the state everyone follows
	bServerOpen = bOpen;
}

void UTHDoorSyncComponent::TickClient(bool bOpen)
{
	const double Now = GetWorld()->GetTimeSeconds();

	if (bOpen != bLastSeenOpen)
	{
		// This machine's player pressed it: the template has already started moving the door here
		bLastSeenOpen = bOpen;
		PendingUntil = Now + RequestTimeout;
		if (ATHCharacter* Character = Cast<ATHCharacter>(GetLocalPawn()))
		{
			Character->ServerRequestDoor(this, bOpen);
		}
		return;
	}

	if (PendingUntil >= 0.0)
	{
		if (bServerOpen != bOpen && Now < PendingUntil)
		{
			return;
		}
		PendingUntil = -1.0;
	}

	if (bServerOpen != bOpen && !IsDoorMoving())
	{
		RunDoorInteract(GetLocalPawn());
		// Read back so the next tick does not take this for the player's own press
		bLastSeenOpen = IsDoorOpen();
	}
}

void UTHDoorSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SyncUnlock();
	if (GetOwner()->HasAuthority())
	{
		TickServer(IsDoorOpen());
	}
	else
	{
		TickClient(IsDoorOpen());
	}
}

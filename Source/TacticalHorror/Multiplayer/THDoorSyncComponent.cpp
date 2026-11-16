#include "Multiplayer/THDoorSyncComponent.h"

#include "Components/TimelineComponent.h"
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
	DOREPLIFETIME(UTHDoorSyncComponent, bServerSlam);
	DOREPLIFETIME(UTHDoorSyncComponent, ServerSwing);
}

double UTHDoorSyncComponent::GetSwing() const
{
	if (const FDoubleProperty* AsDouble = CastField<FDoubleProperty>(SwingProperty))
	{
		return AsDouble->GetPropertyValue_InContainer(GetOwner());
	}
	if (const FFloatProperty* AsFloat = CastField<FFloatProperty>(SwingProperty))
	{
		return AsFloat->GetPropertyValue_InContainer(GetOwner());
	}
	return 0.0;
}

void UTHDoorSyncComponent::SetSwing(double Value)
{
	if (const FDoubleProperty* AsDouble = CastField<FDoubleProperty>(SwingProperty))
	{
		AsDouble->SetPropertyValue_InContainer(GetOwner(), Value);
	}
	else if (const FFloatProperty* AsFloat = CastField<FFloatProperty>(SwingProperty))
	{
		AsFloat->SetPropertyValue_InContainer(GetOwner(), static_cast<float>(Value));
	}
}

double UTHDoorSyncComponent::SwingFrom(const FVector& Where) const
{
	// BP_Outward_Door: Dot(Normal(door - pawn), forward - right); 0 and up swings +85, below -85
	const AActor* Door = GetOwner();
	return FVector::DotProduct((Door->GetActorLocation() - Where).GetSafeNormal(), Door->GetActorForwardVector() - Door->GetActorRightVector());
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
	SwingProperty = FindFProperty<FProperty>(DoorClass, TEXT("DotProduct"));
	if (SwingProperty && !SwingProperty->IsA<FDoubleProperty>() && !SwingProperty->IsA<FFloatProperty>())
	{
		SwingProperty = nullptr;
	}

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
		ServerSwing = static_cast<float>(GetSwing());
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
		// An enemy asking isn't the template's player: this machine's player stands in (the Blueprint reads its HUD), and
		// the swing is set from the enemy afterwards
		APawn* Given = Player && Player->IsA(It->PropertyClass) ? Player : GetLocalPawn();
		if (Given && Given->IsA(It->PropertyClass))
		{
			It->SetObjectPropertyValue_InContainer(Arguments.GetData(), Given);
		}
	}
	GetOwner()->ProcessEvent(InteractFunction, Arguments.GetData());
	InteractFunction->DestroyStruct(Arguments.GetData());
}

void UTHDoorSyncComponent::SetSwingRate(float Rate)
{
	TInlineComponentArray<UTimelineComponent*> Timelines(GetOwner());
	for (UTimelineComponent* Timeline : Timelines)
	{
		Timeline->SetPlayRate(Rate);
	}
	bSpedUp = Rate > 1.0f;
}

bool UTHDoorSyncComponent::HandleRequest(APawn* InRequester, bool bOpen, bool bSlam)
{
	// A charge asks from further off (it is coming fast)
	if (!GetOwner()->HasAuthority() || !InRequester || IsLocked()
		|| FVector::Dist(InRequester->GetActorLocation(), GetOwner()->GetActorLocation()) > MaxRequestDistance * (bSlam ? 1.6f : 1.0f))
	{
		return false;
	}
	bHasRequest = true;
	bRequestedOpen = bOpen;
	bRequestedSlam = bSlam && bOpen;
	Requester = InRequester;
	return bServerOpen != bOpen;
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
			bServerSlam = bRequestedSlam;
			SetSwingRate(bServerSlam ? SlamPlayRate : 1.0f);
			RunDoorInteract(Requester.IsValid() ? Requester.Get() : GetLocalPawn());
			bOpen = IsDoorOpen();
			// Opened: away from whoever asked (a player or an enemy), as the Blueprint means it to
			if (bOpen && SwingProperty && Requester.IsValid())
			{
				SetSwing(SwingFrom(Requester->GetActorLocation()));
			}
		}
	}
	// Whatever moved it (the request above, or the host's own press), this is now the state everyone follows
	bServerOpen = bOpen;
	if (SwingProperty)
	{
		ServerSwing = static_cast<float>(GetSwing());
	}
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

	// The server's swing, whoever this machine had press it (the timeline reads it every frame, so a swing under way turns)
	if (SwingProperty && GetSwing() != ServerSwing)
	{
		SetSwing(ServerSwing);
	}

	if (bServerOpen != bOpen && !IsDoorMoving())
	{
		// Slammed open on the server: fast here too
		SetSwingRate(bServerSlam && bServerOpen ? SlamPlayRate : 1.0f);
		RunDoorInteract(GetLocalPawn());
		// Read back so the next tick does not take this for the player's own press
		bLastSeenOpen = IsDoorOpen();
	}
}

void UTHDoorSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SyncUnlock();
	// A slam over: the door swings at its own pace again
	if (bSpedUp && !IsDoorMoving())
	{
		SetSwingRate(1.0f);
	}
	if (GetOwner()->HasAuthority())
	{
		TickServer(IsDoorOpen());
	}
	else
	{
		TickClient(IsDoorOpen());
	}
}

#include "Range/THTargetCarrier.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "TacticalHorror.h"

ATHTargetCarrier::ATHTargetCarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// It has nothing to see, which would make it irrelevant to clients
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ATHTargetCarrier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATHTargetCarrier, State);
}

void ATHTargetCarrier::BeginPlay()
{
	Super::BeginPlay();
	Axis = Axis.GetSafeNormal();
	// A part attached to another (the range's paper targets hang from their hangers) already goes with it: moving it as
	// well would move it twice, a frame apart
	Parts.RemoveAll([this](const AActor* Part)
	{
		for (const AActor* Parent = Part ? Part->GetAttachParentActor() : nullptr; Parent; Parent = Parent->GetAttachParentActor())
		{
			if (Parts.Contains(Parent))
			{
				return true;
			}
		}
		return Part == nullptr;
	});
	for (AActor* Part : Parts)
	{
		StartLocations.Add(Part ? Part->GetActorLocation() : FVector::ZeroVector);
		USceneComponent* Root = Part ? Part->GetRootComponent() : nullptr;
		if (Root && Root->Mobility != EComponentMobility::Movable)
		{
			UE_LOG(LogTacticalHorror, Warning, TEXT("%s: %s is not Movable, so it can't move (setup_target_carriers.py sets it)"), *GetName(), *Part->GetName());
		}
	}
	// Joining late: the replicated state has arrived already
	Offset = State.Offset;
	ApplyOffset();
}

void ATHTargetCarrier::ServerStartMoving(int8 Direction, const AActor* By)
{
	if (Mover.IsValid() && Mover.Get() != By)
	{
		return;
	}
	Mover = By;
	State.Direction = FMath::Sign(Direction);
	State.Offset = Offset;
	ForceNetUpdate();
}

void ATHTargetCarrier::ServerStopMoving(const AActor* By)
{
	if (Mover.Get() == By)
	{
		StopOnServer();
	}
}

void ATHTargetCarrier::StopOnServer()
{
	Mover = nullptr;
	State.Direction = 0;
	State.Offset = Offset;
	ForceNetUpdate();
}

void ATHTargetCarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State.Direction == 0)
	{
		return;
	}
	Offset = FMath::Clamp(Offset + State.Direction * Speed * DeltaSeconds, MinOffset, MaxOffset);
	ApplyOffset();
	// At the end of the rail, or the player holding the button dropped out without letting go
	if (HasAuthority() && (Offset <= MinOffset || Offset >= MaxOffset || !Mover.IsValid()))
	{
		StopOnServer();
	}
}

void ATHTargetCarrier::OnRep_State()
{
	// Each change starts from the server's position, so the machines can't drift apart
	Offset = State.Offset;
	ApplyOffset();
}

void ATHTargetCarrier::ApplyOffset()
{
	for (int32 i = 0; i < Parts.Num() && i < StartLocations.Num(); ++i)
	{
		if (Parts[i])
		{
			Parts[i]->SetActorLocation(StartLocations[i] + Axis * Offset);
		}
	}
}

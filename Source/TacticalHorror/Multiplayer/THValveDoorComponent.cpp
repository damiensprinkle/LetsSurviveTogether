#include "Multiplayer/THValveDoorComponent.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TimelineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/THCharacter.h"
#include "Sound/SoundBase.h"

UTHValveDoorComponent::UTHValveDoorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UTHValveDoorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTHValveDoorComponent, State);
}

void UTHValveDoorComponent::BeginPlay()
{
	Super::BeginPlay();
	DisableBlueprintOverlap(TEXT("Box"));
	Timeline = FindOwnerComponent<UTimelineComponent>(TEXT("ValveAnimation"));

	// Only the wheel shows the button prompt. The door can't be used, but it's the same actor, so it would show the
	// wheel's prompt too. The interact trace is Visibility; bullets use the Weapon channel, so they still hit the door.
	TInlineComponentArray<UPrimitiveComponent*> Parts(GetOwner());
	for (UPrimitiveComponent* Part : Parts)
	{
		if (Part->GetFName() != TEXT("Valve"))
		{
			Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

float UTHValveDoorComponent::GetPosition() const
{
	return Timeline ? Timeline->GetPlaybackPosition() : 0.0f;
}

float UTHValveDoorComponent::GetLength() const
{
	return Timeline ? Timeline->GetTimelineLength() : 0.0f;
}

bool UTHValveDoorComponent::IsPlayerAtValve(const AActor* Player) const
{
	const UBoxComponent* Box = FindOwnerComponent<UBoxComponent>(TEXT("Box"));
	if (!Player || !Box)
	{
		return false;
	}
	// The player's whole capsule, as the Blueprint's overlap had it: the box is up by the wheel, above a standing player's middle
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Player->GetSimpleCollisionCylinder(Radius, HalfHeight);
	return Box->Bounds.GetBox().Intersect(FBox::BuildAABB(Player->GetActorLocation(), FVector(Radius, Radius, HalfHeight)));
}

void UTHValveDoorComponent::ServerPressed(ATHCharacter* By)
{
	if (!Timeline || !IsPlayerAtValve(By) || Holder.IsValid())
	{
		return;
	}
	Holder = By;
	const float Length = Timeline->GetTimelineLength();
	State.Position = Timeline->GetPlaybackPosition();
	// The template's test: wound all the way, so this hold unwinds it
	State.Direction = State.Position >= Length - KINDA_SMALL_NUMBER ? -1 : 1;
	ApplyState(false);
}

void UTHValveDoorComponent::ServerReleased(ATHCharacter* By)
{
	if (Holder.Get() == By)
	{
		StopServerTurning();
	}
}

void UTHValveDoorComponent::StopServerTurning()
{
	Holder = nullptr;
	if (Timeline)
	{
		Timeline->Stop();
		State.Position = Timeline->GetPlaybackPosition();
	}
	State.Direction = 0;
	StopSounds();
}

void UTHValveDoorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->HasAuthority() && Timeline && State.Direction != 0)
	{
		// Reached the end, or the holder left or dropped out without letting go
		if (!Timeline->IsPlaying() || !Holder.IsValid() || !IsPlayerAtValve(Holder.Get()))
		{
			StopServerTurning();
		}
	}
}

void UTHValveDoorComponent::OnRep_State()
{
	ApplyState(true);
}

void UTHValveDoorComponent::ApplyState(bool bSetPosition)
{
	if (!Timeline)
	{
		return;
	}
	if (bSetPosition)
	{
		// Runs the Blueprint's Update, which turns the wheel and the door
		Timeline->SetPlaybackPosition(State.Position, false, true);
	}
	StopSounds();
	if (State.Direction > 0)
	{
		TurnSound = UGameplayStatics::SpawnSoundAtLocation(this, TurnOpenSound, GetOwner()->GetActorLocation());
		UGameplayStatics::PlaySoundAtLocation(this, DoorMoveSound, GetOwner()->GetActorLocation());
		Timeline->Play();
	}
	else if (State.Direction < 0)
	{
		TurnSound = UGameplayStatics::SpawnSoundAtLocation(this, TurnBackSound, GetOwner()->GetActorLocation());
		Timeline->Reverse();
	}
	else
	{
		Timeline->Stop();
	}
}

void UTHValveDoorComponent::StopSounds()
{
	if (TurnSound)
	{
		TurnSound->Stop();
		TurnSound = nullptr;
	}
}

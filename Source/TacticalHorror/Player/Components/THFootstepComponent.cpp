#include "Player/Components/THFootstepComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UTHFootstepComponent::UTHFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UTHFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ACharacter>(GetOwner());
	// Nobody listens on a dedicated server
	SetComponentTickEnabled(Character.IsValid() && GetNetMode() != NM_DedicatedServer);
}

void UTHFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Owner = Character.Get();
	const UCharacterMovementComponent* Movement = Owner ? Owner->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	// Velocity and movement mode replicate, so this also runs for other players' characters
	const FVector Velocity = Owner->GetVelocity();
	if (Movement->IsFalling())
	{
		FallSpeed = FMath::Max(FallSpeed, -Velocity.Z);
		bWasFalling = true;
		return;
	}

	if (bWasFalling)
	{
		bWasFalling = false;
		if (Movement->IsMovingOnGround() && FallSpeed >= MinLandSpeed)
		{
			PlayStep(EStep::Land);
			DistanceSinceStep = 0.0f;
		}
		FallSpeed = 0.0f;
	}

	const float Speed = Velocity.Size2D();
	if (!Movement->IsMovingOnGround() || Speed < MinStepSpeed)
	{
		// Standing still: the next step lands soon after moving again, not a full stride later
		DistanceSinceStep = FMath::Max(DistanceSinceStep, StrideRange.X * 0.5f);
		return;
	}

	DistanceSinceStep += Speed * DeltaTime;
	const float Stride = FMath::GetMappedRangeValueClamped(SpeedRange, StrideRange, Speed);
	if (DistanceSinceStep >= Stride)
	{
		DistanceSinceStep = 0.0f;
		PlayStep(Owner->bIsCrouched ? EStep::Crouch : Speed < QuietSpeed ? EStep::Quiet : Speed > SprintSpeed ? EStep::Sprint : EStep::Walk);
	}
}

const FTHFootstepSounds& UTHFootstepComponent::GetSoundsUnderFeet() const
{
	const ACharacter* Owner = Character.Get();
	const float HalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start - FVector(0.0f, 0.0f, HalfHeight + 30.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(THFootstep), false, Owner);
	Params.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
		if (const FTHFootstepSounds* Sounds = SurfaceSounds.Find(Surface))
		{
			return *Sounds;
		}
	}
	return DefaultSounds;
}

void UTHFootstepComponent::PlayStep(EStep Kind)
{
	const FTHFootstepSounds& Sounds = GetSoundsUnderFeet();
	USoundBase* Sound = nullptr;
	switch (Kind)
	{
	case EStep::Walk: Sound = Sounds.Walk; break;
	case EStep::Sprint: Sound = Sounds.Sprint ? Sounds.Sprint : Sounds.Walk; break;
	case EStep::Crouch:
	case EStep::Quiet: Sound = Sounds.Crouch ? Sounds.Crouch : Sounds.Walk; break;
	case EStep::Land: Sound = Sounds.Land; break; // No fallback: leave Land empty if something else already plays landings
	}
	if (!Sound)
	{
		return;
	}

	const ACharacter* Owner = Character.Get();
	const FVector Feet = Owner->GetActorLocation() - FVector(0.0f, 0.0f, Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const float Volume = Kind == EStep::Crouch ? CrouchVolume : Kind == EStep::Quiet ? QuietVolume : 1.0f;
	if (Owner->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(this, Sound, LocalVolume * Volume);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Feet, Volume, 1.0f, 0.0f, Attenuation);
	}
}

#include "Player/Components/THFootstepComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Perception/AISense_Hearing.h"

UTHFootstepComponent::UTHFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UTHFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ACharacter>(GetOwner());
	// Also on a dedicated server, where nobody listens but enemies hear the steps (PlayStep)
	SetComponentTickEnabled(Character.IsValid());
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

	const bool bLanded = UpdateFeet(DeltaTime);
	const float Speed = Velocity.Size2D();
	if (!Movement->IsMovingOnGround() || Speed < MinStepSpeed)
	{
		// Standing still: the next step lands soon after moving again, not a full stride later
		DistanceSinceStep = FMath::Max(DistanceSinceStep, StrideRange.X * 0.5f);
		return;
	}

	DistanceSinceStep += Speed * DeltaTime;
	const float Stride = FMath::GetMappedRangeValueClamped(SpeedRange, StrideRange, Speed);
	// By the feet: as one lands (not twice in a blink), or a shuffle's step once well past a stride; else every stride
	const bool bByFeet = !TrackedFeet.IsEmpty();
	const bool bFootStep = bByFeet && bLanded && GetWorld()->GetTimeSeconds() - LastStepTime >= 0.15;
	if (bFootStep || DistanceSinceStep >= (bByFeet ? Stride * 1.5f : Stride))
	{
		DistanceSinceStep = 0.0f;
		FootLandingCount += bFootStep ? 1 : 0;
		PlayStep(Owner->bIsCrouched ? EStep::Crouch : Speed < QuietSpeed ? EStep::Quiet : Speed > SprintSpeed ? EStep::Sprint : EStep::Walk);
	}
}

void UTHFootstepComponent::SetFootMesh(USkeletalMeshComponent* Mesh)
{
	FootMesh = Mesh;
	TrackedFeet.Reset();
	for (const FName& Bone : FootBones)
	{
		if (Mesh && Mesh->GetBoneIndex(Bone) != INDEX_NONE)
		{
			TrackedFeet.Add({ Bone });
		}
	}
}

bool UTHFootstepComponent::UpdateFeet(float DeltaTime)
{
	const USkeletalMeshComponent* Mesh = FootMesh.Get();
	const ACharacter* Owner = Character.Get();
	if (!Mesh || !Owner || TrackedFeet.IsEmpty())
	{
		return false;
	}
	const float Bottom = Owner->GetActorLocation().Z - Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	bool bLanded = false;
	for (FFoot& Foot : TrackedFeet)
	{
		const float Height = Mesh->GetSocketLocation(Foot.Bone).Z - Bottom;
		// The lowest lately, creeping up 20 cm a second (a step up, a slope) so it follows the floor
		Foot.Floor = Foot.bHasFloor ? FMath::Min(Height, Foot.Floor + 20.0f * DeltaTime) : Height;
		Foot.bHasFloor = true;
		if (Height > Foot.Floor + FootLiftHeight)
		{
			Foot.bLifted = true;
		}
		else if (Foot.bLifted && Height <= Foot.Floor + FootPlantHeight)
		{
			Foot.bLifted = false;
			bLanded = true;
		}
	}
	return bLanded;
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
	const ACharacter* Owner = Character.Get();
	const FVector Feet = Owner->GetActorLocation() - FVector(0.0f, 0.0f, Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	LastStepTime = GetWorld()->GetTimeSeconds();
	++StepCount;

	// Enemies hear steps (AI hearing, server only), whether or not the surface has a sound
	if (bMakesNoise && Owner->HasAuthority())
	{
		const float Loudness = Kind == EStep::Sprint ? SprintNoise : Kind == EStep::Walk ? WalkNoise : Kind == EStep::Land ? LandNoise
			: Kind == EStep::Quiet ? QuietNoise : CrouchNoise;
		UAISense_Hearing::ReportNoiseEvent(this, Feet, Loudness, const_cast<ACharacter*>(Owner), 0.0f, TEXT("Footstep"));
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

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
	// A body with steps of its own (a charger's stomp; its look's Footstep Sounds), whatever the floor
	const bool bOverride = !StepSoundOverride.IsEmpty() && (Kind == EStep::Walk || Kind == EStep::Sprint);
	if (bOverride)
	{
		Sound = StepSoundOverride[FMath::RandHelper(StepSoundOverride.Num())];
	}
	if (!Sound)
	{
		return;
	}

	const float Volume = (Kind == EStep::Crouch ? CrouchVolume : Kind == EStep::Quiet ? QuietVolume : 1.0f) * (bOverride ? StepOverrideVolume : 1.0f);
	if (Owner->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(this, Sound, LocalVolume * Volume);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Feet, Volume, 1.0f, 0.0f, Attenuation);
	}
}

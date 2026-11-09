#include "Weapons/THShellCasing.h"
#include "Weapons/THWeaponData.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Sound/SoundBase.h"

ATHShellCasing::ATHShellCasing()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	// The physics body is a capsule sized from the mesh, so casings collide whether or not the mesh has simple collision
	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	Collision->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Collision->SetCollisionObjectType(ECC_PhysicsBody);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->BodyInstance.bUseCCD = true; // small and fast: without CCD casings drop through thin floors
	Collision->SetCanEverAffectNavigation(false);
	// A round capsule on a smooth floor would roll for metres; strong angular damping lets it settle near where it lands
	Collision->SetLinearDamping(0.3f);
	Collision->SetAngularDamping(6.0f);
	Collision->SetCapsuleSize(0.6f, 1.2f);
	RootComponent = Collision;

	// Brass on concrete: little bounce, enough friction to stop sliding
	PhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("PhysicalMaterial"));
	PhysicalMaterial->Friction = 0.9f;
	PhysicalMaterial->Restitution = 0.15f;
	PhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
	PhysicalMaterial->bOverrideRestitutionCombineMode = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);
}

void ATHShellCasing::Launch(const UTHWeaponData* Data, const FVector& Velocity, const FVector& AngularVelocityDegrees)
{
	WeaponData = Data;
	Mesh->SetStaticMesh(Data->ShellMesh);
	SetActorScale3D(FVector(Data->ShellScale));
	SetLifeSpan(FMath::Max(Data->ShellLifetime, 0.1f));

	// Fit the capsule to the mesh: its axis (Z) along the casing's length, which the casing meshes model along Z
	if (const UStaticMesh* StaticMesh = Data->ShellMesh)
	{
		const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
		const float Radius = FMath::Max(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y), 0.2f);
		Collision->SetCapsuleSize(Radius, FMath::Max(Bounds.BoxExtent.Z, Radius));
		Mesh->SetRelativeLocation(-Bounds.Origin);
	}

	Collision->SetPhysMaterialOverride(PhysicalMaterial);
	Collision->OnComponentHit.AddDynamic(this, &ATHShellCasing::OnMeshHit);
	Collision->SetSimulatePhysics(true);
	Collision->SetPhysicsLinearVelocity(Velocity);
	Collision->SetPhysicsAngularVelocityInDegrees(AngularVelocityDegrees);
}

void ATHShellCasing::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// One clink per casing: the first proper impact. Later bounces and rolling stay quiet so sprays don't turn into noise.
	if (bPlayedBounceSound || !WeaponData || WeaponData->ShellBounceSounds.IsEmpty())
	{
		return;
	}
	bPlayedBounceSound = true;

	USoundBase* Sound = WeaponData->ShellBounceSounds[FMath::RandRange(0, WeaponData->ShellBounceSounds.Num() - 1)];
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.ImpactPoint,
			WeaponData->ShellBounceVolume * UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this), FMath::FRandRange(0.95f, 1.05f),
			0.0f, WeaponData->ShellBounceAttenuation);
	}
}

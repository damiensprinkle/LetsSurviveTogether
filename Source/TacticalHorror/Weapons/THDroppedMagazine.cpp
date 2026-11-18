#include "Weapons/THDroppedMagazine.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

ATHDroppedMagazine::ATHDroppedMagazine()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	// A box fitted to the mesh is the physics body, so it works whether or not the mesh has simple collision
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Collision->SetCollisionObjectType(ECC_PhysicsBody);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->BodyInstance.bUseCCD = true;
	Collision->SetCanEverAffectNavigation(false);
	Collision->SetLinearDamping(0.2f);
	Collision->SetAngularDamping(2.0f);
	RootComponent = Collision;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(Collision);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCanEverAffectNavigation(false);
}

void ATHDroppedMagazine::Drop(UStaticMesh* Mesh, const FVector& Velocity, float Lifetime, USoundBase* HitSound)
{
	DropSound = HitSound;
	MeshComponent->SetStaticMesh(Mesh);
	SetLifeSpan(FMath::Max(Lifetime, 0.1f));

	if (Mesh)
	{
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		Collision->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(0.1f)), false);
		// The box is centred on the actor; shift the mesh so its middle sits there
		MeshComponent->SetRelativeLocation(-Bounds.Origin);
	}

	// Falling from the third-person gun, which the holder doesn't see: crouched in first person that's just under their
	// eyes, so it would fall through their view. They see it once it lands (or after a second, should it not).
	MeshComponent->SetOwnerNoSee(true);
	FTimerHandle Reveal;
	GetWorldTimerManager().SetTimer(Reveal, this, &ATHDroppedMagazine::ShowToHolder, 1.0f, false);
	Collision->OnComponentHit.AddDynamic(this, &ATHDroppedMagazine::OnMeshHit);
	Collision->SetSimulatePhysics(true);
	Collision->SetPhysicsLinearVelocity(Velocity);
	Collision->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-90.0f, 90.0f), FMath::FRandRange(-90.0f, 90.0f), FMath::FRandRange(-60.0f, 60.0f)));
}

void ATHDroppedMagazine::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ShowToHolder();
	if (bPlayedSound || !DropSound)
	{
		return;
	}
	bPlayedSound = true;
	UGameplayStatics::PlaySoundAtLocation(this, DropSound, Hit.ImpactPoint, DropVolume * UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this), FMath::FRandRange(0.95f, 1.05f));
}

void ATHDroppedMagazine::ShowToHolder()
{
	MeshComponent->SetOwnerNoSee(false);
}

bool ATHDroppedMagazine::IsShownToHolder() const
{
	return !MeshComponent->bOwnerNoSee;
}

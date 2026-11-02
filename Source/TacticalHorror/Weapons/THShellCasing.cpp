#include "Weapons/THShellCasing.h"
#include "Weapons/THWeaponData.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ATHShellCasing::ATHShellCasing()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->BodyInstance.bUseCCD = true; // small and fast: without CCD casings drop through thin floors
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetLinearDamping(0.2f);
	Mesh->SetAngularDamping(1.0f);
	RootComponent = Mesh;
}

void ATHShellCasing::Launch(const UTHWeaponData* Data, const FVector& Velocity, const FVector& AngularVelocityDegrees)
{
	WeaponData = Data;
	Mesh->SetStaticMesh(Data->ShellMesh);
	SetActorScale3D(FVector(Data->ShellScale));
	SetLifeSpan(FMath::Max(Data->ShellLifetime, 0.1f));

	Mesh->OnComponentHit.AddDynamic(this, &ATHShellCasing::OnMeshHit);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(AngularVelocityDegrees);
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
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.ImpactPoint, WeaponData->ShellBounceVolume, FMath::FRandRange(0.95f, 1.05f),
			0.0f, WeaponData->ShellBounceAttenuation);
	}
}

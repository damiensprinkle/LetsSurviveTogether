#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponData.h"
#include "Weapons/THWeaponComponent.h"
#include "Weapons/THShellCasing.h"
#include "Weapons/THDroppedMagazine.h"
#include "Weapons/THImpactSurfaces.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Player/THBodyAnimInstance.h"
#include "Player/THBodyAnimSet.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarMuzzleFlashQuality(
	TEXT("th.MuzzleFlashQuality"), 1, TEXT("Muzzle flash brightness: 0 = low, 1 = medium, 2 = high."));
static TAutoConsoleVariable<int32> CVarDebugThirdPerson(
	TEXT("th.DebugThirdPersonWeapon"), 0, TEXT("1 = log where the third-person weapon copy is placed every frame; 2 = also draw it with the default material."));
static TAutoConsoleVariable<FString> CVarThirdPersonTune(
	TEXT("th.ThirdPersonTune"), TEXT(""),
	TEXT("Live tuning of the held weapon's third-person hold, added to its data asset values (copy the result into the asset): ")
	TEXT("\"anchorX anchorY anchorZ  rightX rightY rightZ  rightPitch rightYaw rightRoll  leftX leftY leftZ  leftPitch leftYaw leftRoll  ")
	TEXT("rightShoulderX rightShoulderY rightShoulderZ  leftShoulderX leftShoulderY leftShoulderZ\". Missing values = 0."));
static TAutoConsoleVariable<FString> CVarWeaponLightTune(
	TEXT("th.WeaponLightTune"), TEXT(""),
	TEXT("Live tuning of the held weapon's under-barrel light, added to its data asset's Weapon Light Attach (copy the result into the asset): ")
	TEXT("\"x y z  pitch yaw roll\" in the muzzle bone's space. Missing values = 0."));
#include "TacticalHorror.h"

namespace
{
	/** The held weapon's third-person hold values plus th.ThirdPersonTune. */
	struct FThirdPersonHold
	{
		FVector Anchor;
		FVector RightOffset;
		FRotator RightRotation;
		FVector LeftOffset;
		FRotator LeftRotation;
		FVector RightShoulder;
		FVector LeftShoulder;
	};

	FThirdPersonHold GetThirdPersonHold(const UTHWeaponData& Data)
	{
		FThirdPersonHold Hold{ Data.ThirdPersonAnchorOffset, Data.ThirdPersonRightHandOffset, Data.ThirdPersonRightHandRotation,
			Data.ThirdPersonLeftHandOffset, Data.ThirdPersonLeftHandRotation, Data.ThirdPersonRightShoulderOffset, Data.ThirdPersonLeftShoulderOffset };
		TArray<FString> Parts;
		CVarThirdPersonTune.GetValueOnGameThread().ParseIntoArrayWS(Parts);
		if (!Parts.IsEmpty())
		{
			float V[21] = {};
			for (int32 Index = 0; Index < FMath::Min(Parts.Num(), 21); ++Index)
			{
				V[Index] = FCString::Atof(*Parts[Index]);
			}
			Hold.Anchor += FVector(V[0], V[1], V[2]);
			Hold.RightOffset += FVector(V[3], V[4], V[5]);
			Hold.RightRotation += FRotator(V[6], V[7], V[8]);
			Hold.LeftOffset += FVector(V[9], V[10], V[11]);
			Hold.LeftRotation += FRotator(V[12], V[13], V[14]);
			Hold.RightShoulder += FVector(V[15], V[16], V[17]);
			Hold.LeftShoulder += FVector(V[18], V[19], V[20]);
		}
		return Hold;
	}
}

ATHWeaponBase::ATHWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicatingMovement(false);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->bCastDynamicShadow = false;
	Mesh->CastShadow = false;
	Mesh->SetOnlyOwnerSee(true);
	RootComponent = Mesh;

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ThirdPersonMesh"));
	ThirdPersonMesh->SetupAttachment(Mesh);
	ThirdPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonMesh->SetOwnerNoSee(true);
	// Its grip bone is read every frame to place it, so keep the (paused, cheap) pose current even before it is first seen
	ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Mesh);
	MuzzleLight->SetVisibility(false);
	MuzzleLight->SetAttenuationRadius(600.0f);
	MuzzleLight->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleLight->SetCastShadows(false);
	MuzzleLight->bAffectsWorld = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Quad = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("MuzzleFlashQuad%d"), Index));
		Quad->SetupAttachment(Mesh);
		Quad->SetStaticMesh(PlaneMesh.Object);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCastShadow(false);
		Quad->SetVisibility(false);
		MuzzleFlashQuads.Add(Quad);
	}

	WeaponLightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponLightMesh"));
	WeaponLightMesh->SetupAttachment(Mesh);
	WeaponLightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponLightMesh->SetCastShadow(false);
	WeaponLightMesh->SetOnlyOwnerSee(true);
	WeaponLightMesh->SetVisibility(false);

	ThirdPersonWeaponLightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonWeaponLightMesh"));
	ThirdPersonWeaponLightMesh->SetupAttachment(ThirdPersonMesh);
	ThirdPersonWeaponLightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonWeaponLightMesh->SetOwnerNoSee(true);
	ThirdPersonWeaponLightMesh->SetVisibility(false);

	WeaponLightBeam = CreateDefaultSubobject<USpotLightComponent>(TEXT("WeaponLightBeam"));
	WeaponLightBeam->SetupAttachment(Mesh);
	WeaponLightBeam->SetIntensityUnits(ELightUnits::Unitless);
	WeaponLightBeam->SetVisibility(false);

	HeldMagazine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldMagazine"));
	HeldMagazine->SetupAttachment(Mesh);
	HeldMagazine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldMagazine->SetOwnerNoSee(true);
	HeldMagazine->SetHiddenInGame(true);
	HeldMagazine->SetUsingAbsoluteLocation(true);
	HeldMagazine->SetUsingAbsoluteRotation(true);
	HeldMagazine->SetUsingAbsoluteScale(true);
}

void ATHWeaponBase::ResetThirdPersonMagazine()
{
	if (bMagazineBoneHidden && WeaponData)
	{
		ThirdPersonMesh->UnHideBoneByName(WeaponData->ThirdPersonMagazineBone);
	}
	bMagazineBoneHidden = false;
	bMagazineDropped = false;
	ReloadStartTime = 0.0;
	HeldMagazine->SetHiddenInGame(true);
}

void ATHWeaponBase::UpdateThirdPersonMagazine()
{
	if (!WeaponData || WeaponData->ThirdPersonMagazineBone.IsNone() || !UsesThirdPersonWeaponCopy())
	{
		return;
	}
	if (!bIsReloading || !bIsEquipped || ReloadStartTime <= 0.0)
	{
		if (ReloadStartTime > 0.0 || bMagazineBoneHidden)
		{
			ResetThirdPersonMagazine();
		}
		return;
	}

	const FName Bone = WeaponData->ThirdPersonMagazineBone;
	const float Phase = (GetWorld()->GetTimeSeconds() - ReloadStartTime) / FMath::Max(WeaponData->ReloadDuration, 0.01f);

	// Out of the gun and onto the floor
	if (!bMagazineDropped && Phase >= WeaponData->MagazineDropAt)
	{
		bMagazineDropped = true;
		const FTransform Dropped = ThirdPersonMesh->GetSocketTransform(Bone);
		ThirdPersonMesh->HideBoneByName(Bone, PBO_None);
		bMagazineBoneHidden = true;

		if (WeaponData->ThirdPersonMagazineMesh && ThirdPersonBody.IsValid() && !ThirdPersonMesh->bHiddenInGame)
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			// The dropped actor is centred on the mesh's middle (its physics box), so start it where that middle is now
			const FVector Centre = Dropped.TransformPosition(WeaponData->ThirdPersonMagazineMesh->GetBounds().Origin);
			if (ATHDroppedMagazine* Magazine = GetWorld()->SpawnActor<ATHDroppedMagazine>(Centre, Dropped.Rotator(), Params))
			{
				Magazine->SetActorScale3D(Dropped.GetScale3D());
				const APawn* Pawn = GetOwningPawn();
				const FVector Velocity = (Pawn ? Pawn->GetVelocity() : FVector::ZeroVector) + FVector(0.0f, 0.0f, -60.0f);
				Magazine->Drop(WeaponData->ThirdPersonMagazineMesh, Velocity, WeaponData->DroppedMagazineLifetime, WeaponData->MagazineDropSound);
			}
		}
	}

	// The new one in the left hand from the waist until it is seated
	bool bHolding = false;
	USkeletalMeshComponent* Body = ThirdPersonBody.Get();
	const UTHBodyAnimSet* BodySet = WeaponData->BodyAnimSet;
	if (Body && BodySet && WeaponData->ThirdPersonMagazineMesh && Phase >= WeaponData->MagazineGrabAt && Phase < WeaponData->MagazineSeatAt)
	{
		const USkeletalMesh* BodyMesh = Body->GetSkeletalMeshAsset();
		if (BodyMesh && BodyLeftFingersMesh.Get() != BodyMesh)
		{
			BodyLeftFingersMesh = BodyMesh;
			FTHHandShape::FindFingers(BodyMesh->GetRefSkeleton(), BodySet->LeftHandBone, BodyLeftFingers);
		}
		if (BodyLeftFingers[1].Num() && BodyLeftFingers[2].Num() && BodyLeftFingers[4].Num())
		{
			const FVector W = Body->GetSocketLocation(BodySet->LeftHandBone);
			const FVector I = Body->GetSocketLocation(BodyLeftFingers[1][0]);
			const FVector M = Body->GetSocketLocation(BodyLeftFingers[2][0]);
			const FVector P = Body->GetSocketLocation(BodyLeftFingers[4][0]);
			const FTransform Palm(FTHHandShape::MakeFrame(W, I, M, P), FTHHandShape::MakePalm(W, I, M, P), ThirdPersonMesh->GetComponentScale());
			if (HeldMagazine->GetStaticMesh() != WeaponData->ThirdPersonMagazineMesh)
			{
				HeldMagazine->SetStaticMesh(WeaponData->ThirdPersonMagazineMesh);
			}
			// The rig's magazine bones carry a scale, which the magazine mesh was modelled against: keep it
			HeldMagazine->SetWorldTransform(WeaponData->ThirdPersonMagazineInPalm * Palm);
			bHolding = !ThirdPersonMesh->bHiddenInGame;
		}
	}
	HeldMagazine->SetHiddenInGame(!bHolding);

	// Seated: the gun's own magazine is back
	if (bMagazineBoneHidden && Phase >= WeaponData->MagazineSeatAt)
	{
		ThirdPersonMesh->UnHideBoneByName(Bone);
		bMagazineBoneHidden = false;
	}
}

bool ATHWeaponBase::HasWeaponLight() const
{
	return WeaponData && WeaponData->WeaponLightMesh;
}

bool ATHWeaponBase::IsThirdPersonHiddenForDraw() const
{
	return ThirdPersonRevealTime > GetWorld()->GetTimeSeconds();
}

USoundBase* ATHWeaponBase::GetLightClickSound() const
{
	return WeaponData ? WeaponData->WeaponLightClickSound.Get() : nullptr;
}

void ATHWeaponBase::SetupWeaponLight()
{
	if (!HasWeaponLight())
	{
		return;
	}
	WeaponLightMesh->SetStaticMesh(WeaponData->WeaponLightMesh);
	ThirdPersonWeaponLightMesh->SetStaticMesh(WeaponData->WeaponLightMesh);
	WeaponLightBeam->SetIntensity(WeaponData->WeaponLightIntensity);
	WeaponLightBeam->SetAttenuationRadius(WeaponData->WeaponLightRange);
	WeaponLightBeam->SetInnerConeAngle(WeaponData->WeaponLightInnerCone);
	WeaponLightBeam->SetOuterConeAngle(WeaponData->WeaponLightOuterCone);
	WeaponLightBeam->SetLightColor(WeaponData->WeaponLightColor);
}

void ATHWeaponBase::UpdateWeaponLight()
{
	if (!HasWeaponLight())
	{
		return;
	}

	const APawn* Pawn = GetOwningPawn();
	const UTHWeaponComponent* Weapons = Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	const bool bFitted = bIsEquipped && Weapons && Weapons->IsWeaponLightUnlocked();
	const bool bOn = bFitted && Weapons->IsWeaponLightOn();

	// The data's attach frame plus th.WeaponLightTune, in the muzzle bone's space
	FTransform Attach = WeaponData->WeaponLightAttach;
	TArray<FString> Parts;
	CVarWeaponLightTune.GetValueOnGameThread().ParseIntoArrayWS(Parts);
	if (!Parts.IsEmpty())
	{
		float V[6] = {};
		for (int32 Index = 0; Index < FMath::Min(Parts.Num(), 6); ++Index)
		{
			V[Index] = FCString::Atof(*Parts[Index]);
		}
		Attach.AddToTranslation(FVector(V[0], V[1], V[2]));
		Attach.SetRotation(Attach.GetRotation() * FRotator(V[3], V[4], V[5]).Quaternion());
	}
	const FName Bone = WeaponData->MuzzleSocketName;
	const FTransform MeshRelative = WeaponData->WeaponLightMeshTransform * Attach;

	auto Place = [&](UStaticMeshComponent* LightMesh, USkeletalMeshComponent* Gun)
	{
		if (LightMesh->GetAttachParent() != Gun || LightMesh->GetAttachSocketName() != Bone)
		{
			LightMesh->AttachToComponent(Gun, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);
		}
		LightMesh->SetRelativeTransform(MeshRelative);
		LightMesh->SetVisibility(bFitted);
	};
	Place(WeaponLightMesh, Mesh);
	Place(ThirdPersonWeaponLightMesh, ThirdPersonMesh);
	// The third-person copy is hidden until the draw brings it up; the light goes with it
	ThirdPersonWeaponLightMesh->SetHiddenInGame(ThirdPersonMesh->bHiddenInGame);

	// Lights reach every viewer, so only one beam: on the copy this machine sees
	USkeletalMeshComponent* Visible = GetVisibleMesh();
	if (WeaponLightBeam->GetAttachParent() != Visible || WeaponLightBeam->GetAttachSocketName() != Bone)
	{
		WeaponLightBeam->AttachToComponent(Visible, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);
	}
	WeaponLightBeam->SetRelativeTransform(FTransform(WeaponData->WeaponLightLensOffset) * Attach);
	WeaponLightBeam->SetVisibility(bOn && !(Visible == ThirdPersonMesh && ThirdPersonMesh->bHiddenInGame));

	if (CVarDebugThirdPerson.GetValueOnGameThread() == 4)
	{
		UE_LOG(LogTacticalHorror, Warning, TEXT("%s light: fitted=%d on=%d bone=%s boneIdx=%d | FP mesh=%s vis=%d | TP mesh=%s vis=%d hidden=%d | muzzleTP=%s scale=%s"),
			*GetName(), bFitted, bOn, *Bone.ToString(), ThirdPersonMesh->GetBoneIndex(Bone), *WeaponLightMesh->GetComponentLocation().ToString(), WeaponLightMesh->IsVisible(),
			*ThirdPersonWeaponLightMesh->GetComponentLocation().ToString(), ThirdPersonWeaponLightMesh->IsVisible(), ThirdPersonWeaponLightMesh->bHiddenInGame,
			*ThirdPersonMesh->GetSocketLocation(Bone).ToString(), *ThirdPersonWeaponLightMesh->GetComponentScale().ToString());
	}
}

void ATHWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATHWeaponBase, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATHWeaponBase, ReserveAmmo, COND_OwnerOnly);
	DOREPLIFETIME(ATHWeaponBase, bIsReloading);
	DOREPLIFETIME(ATHWeaponBase, FireModeIndex);
	DOREPLIFETIME_CONDITION(ATHWeaponBase, SpreadSeed, COND_InitialOnly);
}

void ATHWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (!WeaponData)
	{
		UE_LOG(LogTacticalHorror, Error, TEXT("%s has no WeaponData assigned"), *GetName());
		return;
	}

	if (HasAuthority())
	{
		CurrentAmmo = WeaponData->MagazineSize;
		ReserveAmmo = WeaponData->StartingReserveAmmo;
		SpreadSeed = FMath::Rand();
	}

	SetupThirdPersonMesh();
	SetupWeaponLight();

	// On clients the equip notify can arrive before BeginPlay, so don't hide a weapon that is already held
	if (!bIsEquipped)
	{
		SetActorHiddenInGame(true);
	}
}

APawn* ATHWeaponBase::GetOwningPawn() const
{
	return Cast<APawn>(GetOwner());
}

bool ATHWeaponBase::IsLocallyControlledOwner() const
{
	const APawn* Pawn = GetOwningPawn();
	return Pawn && Pawn->IsLocallyControlled();
}

bool ATHWeaponBase::IsAimingDownSights() const
{
	const APawn* Pawn = GetOwningPawn();
	const UTHWeaponComponent* Weapons = Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	return Weapons && Weapons->IsAiming();
}

ETHFireMode ATHWeaponBase::GetFireMode() const
{
	if (!WeaponData || WeaponData->FireModes.IsEmpty())
	{
		return ETHFireMode::SemiAuto;
	}
	return WeaponData->FireModes[FireModeIndex % WeaponData->FireModes.Num()];
}

// Equip

void ATHWeaponBase::OnEquipped()
{
	bIsEquipped = true;
	SetActorHiddenInGame(false);
	AttachToOwner();

	EquipFinishTime = GetWorld()->GetTimeSeconds() + (WeaponData ? WeaponData->EquipDuration : 0.0f);

	// Tick while equipped on every machine: the owner may not be possessed yet when the first weapon is equipped,
	// so whether this is the local player's weapon is decided each frame rather than here
	SetActorTickEnabled(true);
	UpdateFirstPersonRendering();

	ThirdPersonRevealTime = 0.0;
	AttachThirdPersonMesh();

	if (WeaponData)
	{
		PlayWeaponAnimation(WeaponData->EquipAnimation);

		if (const UTHBodyAnimSet* BodySet = WeaponData->BodyAnimSet)
		{
			// Never more than twice as fast as authored, so a short EquipDuration doesn't make it a twitch
			const float Length = BodySet->Equip ? BodySet->Equip->GetPlayLength() : 0.0f;
			const float Duration = FMath::Max(WeaponData->EquipDuration, Length * 0.5f);
			PlayBodyAction(BodySet->Equip, Duration, 0.05f, BodySet->ActionBlendTime);
			// The third-person weapon appears once the draw has brought the hand up
			if (BodySet->Equip)
			{
				ThirdPersonRevealTime = GetWorld()->GetTimeSeconds() + Duration * BodySet->EquipRevealFraction;
			}
		}

		// A freshly drawn weapon comes up ready (aiming pose), then lowers after the set's Ready Hold Time
		const APawn* Pawn = GetOwningPawn();
		const UTHWeaponComponent* Weapons = Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
		if (UTHBodyAnimInstance* Body = Weapons ? Weapons->GetBodyAnimInstance() : nullptr)
		{
			Body->RaiseWeapon();
		}
	}
	ThirdPersonMesh->SetHiddenInGame(!UsesThirdPersonWeaponCopy() || ThirdPersonRevealTime > GetWorld()->GetTimeSeconds());

	ReceiveEquipped();
}

void ATHWeaponBase::OnUnequipped()
{
	StopFire();
	bIsEquipped = false;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	PendingViewRecovery = FVector2D::ZeroVector;

	if (USkeletalMeshComponent* Body = ThirdPersonBody.Get())
	{
		RemoveTickPrerequisiteComponent(Body);
	}
	ThirdPersonBody.Reset();
	ThirdPersonMesh->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (HasAuthority() && bIsReloading)
	{
		GetWorldTimerManager().ClearTimer(ReloadTimer);
		bIsReloading = false;
	}
	UpdateWeaponLight();
	ResetThirdPersonMagazine();

	ReceiveUnequipped();
}

void ATHWeaponBase::AttachToOwner_Implementation()
{
	const APawn* Pawn = GetOwningPawn();
	if (!Pawn)
	{
		return;
	}

	// The horror character can carry more than one camera (night vision), so prefer the active one
	TInlineComponentArray<UCameraComponent*> Cameras(Pawn);
	UCameraComponent* HoldCamera = nullptr;
	for (UCameraComponent* Camera : Cameras)
	{
		if (!HoldCamera || Camera->IsActive())
		{
			HoldCamera = Camera;
		}
	}

	USceneComponent* Parent = HoldCamera ? static_cast<USceneComponent*>(HoldCamera) : Pawn->GetRootComponent();
	AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	if (WeaponData)
	{
		SetActorRelativeTransform(WeaponData->CameraHoldOffset);
	}
}

// Input

void ATHWeaponBase::StartFire()
{
	bWantsToFire = true;
	bFiredThisTriggerPull = false;
	HandleFiring();
}

void ATHWeaponBase::StopFire()
{
	bWantsToFire = false;
}

void ATHWeaponBase::SetFiringBlocked(bool bBlocked)
{
	bFiringBlocked = bBlocked;
	if (bBlocked)
	{
		StopFire();
	}
}

void ATHWeaponBase::StartReload()
{
	if (!WeaponData || bIsReloading || CurrentAmmo >= WeaponData->MagazineSize || ReserveAmmo <= 0)
	{
		return;
	}

	StopFire();

	if (HasAuthority())
	{
		ServerReload_Implementation();
	}
	else
	{
		ServerReload();
	}
}

void ATHWeaponBase::CycleFireMode()
{
	if (!WeaponData || WeaponData->FireModes.Num() < 2)
	{
		return;
	}

	FireModeIndex = (FireModeIndex + 1) % WeaponData->FireModes.Num();
	OnRep_FireModeIndex();

	if (!HasAuthority())
	{
		ServerSetFireModeIndex(FireModeIndex);
	}
}

void ATHWeaponBase::ServerSetFireModeIndex_Implementation(uint8 NewIndex)
{
	if (WeaponData && NewIndex < WeaponData->FireModes.Num())
	{
		FireModeIndex = NewIndex;
	}
}

void ATHWeaponBase::OnRep_FireModeIndex()
{
	ReceiveFireModeChanged(GetFireMode());
}

// Firing (owning client)

void ATHWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateFirstPersonRendering();

	// On clients the weapon can be equipped before its owner has replicated; attach once there is a body
	if (bIsEquipped && !ThirdPersonBody.IsValid())
	{
		AttachThirdPersonMesh();
	}
	UpdateThirdPersonMesh();
	UpdateWeaponLight();
	UpdateThirdPersonMagazine();

	if (IsLocallyControlledOwner())
	{
		HandleFiring();
		UpdateViewRecovery(DeltaSeconds);
	}
}

// Third person

void ATHWeaponBase::SetupThirdPersonMesh()
{
	if (!ThirdPersonMesh->GetSkeletalMeshAsset())
	{
		ThirdPersonMesh->SetSkeletalMeshAsset(Mesh->GetSkeletalMeshAsset());
	}
	USkeletalMesh* SkeletalMesh = ThirdPersonMesh->GetSkeletalMeshAsset();
	if (!SkeletalMesh || !WeaponData)
	{
		return;
	}

	// Hide the first-person arms. Sections are per LOD, each pointing at a material slot.
	if (const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering())
	{
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
		for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
		{
			const TArray<FSkelMeshRenderSection>& Sections = RenderData->LODRenderData[LODIndex].RenderSections;
			const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
			for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
			{
				// A section's material can be remapped per LOD; ShowMaterialSection hides by that remapped slot
				int32 MaterialIndex = Sections[SectionIndex].MaterialIndex;
				if (LODInfo && LODInfo->LODMaterialMap.IsValidIndex(SectionIndex) && LODInfo->LODMaterialMap[SectionIndex] != INDEX_NONE)
				{
					MaterialIndex = LODInfo->LODMaterialMap[SectionIndex];
				}
				const bool bHide = Materials.IsValidIndex(MaterialIndex) && WeaponData->ThirdPersonHiddenMaterialSlots.Contains(Materials[MaterialIndex].MaterialSlotName);
				UE_LOG(LogTacticalHorror, Verbose, TEXT("%s third-person LOD %d section %d material %d (%s): %s"), *GetName(), LODIndex, SectionIndex, MaterialIndex,
					Materials.IsValidIndex(MaterialIndex) ? *Materials[MaterialIndex].MaterialSlotName.ToString() : TEXT("?"), bHide ? TEXT("hidden") : TEXT("shown"));
				if (bHide)
				{
					ThirdPersonMesh->ShowMaterialSection(MaterialIndex, SectionIndex, false, LODIndex);
				}
			}
		}
	}

	for (const FName& Bone : WeaponData->ThirdPersonHiddenBones)
	{
		if (ThirdPersonMesh->GetBoneIndex(Bone) != INDEX_NONE)
		{
			ThirdPersonMesh->HideBoneByName(Bone, PBO_None);
		}
	}

	// Hold one frame: the rest pose of first-person rigs is often not the held pose (the G17's slide is open in it)
	UAnimSequenceBase* Pose = WeaponData->ThirdPersonPose ? WeaponData->ThirdPersonPose.Get() : WeaponData->IdleAnimation.Get();
	if (Pose)
	{
		ThirdPersonMesh->PlayAnimation(Pose, false);
		ThirdPersonMesh->SetPosition(0.0f, false);
		ThirdPersonMesh->Stop();
	}
}

void ATHWeaponBase::AttachThirdPersonMesh()
{
	const APawn* Pawn = GetOwningPawn();
	const UTHWeaponComponent* Weapons = Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	USkeletalMeshComponent* Body = Weapons ? Weapons->GetBodyMesh() : nullptr;
	if (!Body || !WeaponData)
	{
		return;
	}

	if (USkeletalMeshComponent* Previous = ThirdPersonBody.Get(); Previous && Previous != Body)
	{
		RemoveTickPrerequisiteComponent(Previous);
	}
	if (Body != ThirdPersonBody.Get())
	{
		bHasThirdPersonHandOffset = false;
	}
	ThirdPersonBody = Body;
	AddTickPrerequisiteComponent(Body);
	if (!UsesThirdPersonWeaponCopy())
	{
		ThirdPersonMesh->SetHiddenInGame(true);
		return;
	}

	// Attached to the hand so that, between our updates, it moves with it
	ThirdPersonMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponData->ThirdPersonHandBone);
	UpdateThirdPersonMesh();
}

void ATHWeaponBase::UpdateThirdPersonMesh()
{
	const USkeletalMeshComponent* Body = ThirdPersonBody.Get();
	const APawn* Pawn = GetOwningPawn();
	if (!UsesThirdPersonWeaponCopy())
	{
		ThirdPersonMesh->SetHiddenInGame(true);
		return;
	}
	if (!Body || !Pawn || !WeaponData || !bIsEquipped || ThirdPersonMesh->GetAttachParent() != Body)
	{
		return;
	}

	// Hidden until the equip animation has brought the hand up (does nothing when unchanged)
	ThirdPersonMesh->SetHiddenInGame(ThirdPersonRevealTime > GetWorld()->GetTimeSeconds());

	// Barrel along the aim (as in first person, so effects line up), grip bone at the anchor offset from the head.
	// The body's hand IK uses the same formula (FTHWeaponGrip), so the hands land on the grips.
	const FQuat Aim = Body->GetComponentQuat() * FTHWeaponGrip::GetAimInComponentSpace(Pawn, Body, WeaponData->ThirdPersonRotationOffset);
	const FQuat Rotation = Aim * WeaponData->CameraHoldOffset.GetRotation();
	const FVector Scale = ThirdPersonMesh->GetComponentScale();
	const FVector Grip = ThirdPersonMesh->GetBoneLocation(WeaponData->ThirdPersonGripBone, EBoneSpaces::ComponentSpace);
	const FTransform HandTransform = Body->GetSocketTransform(WeaponData->ThirdPersonHandBone);
	UTHBodyAnimInstance* BodyAnim = Cast<UTHBodyAnimInstance>(Body->GetAnimInstance());

	const FThirdPersonHold Hold = GetThirdPersonHold(*WeaponData);
	const FVector GripPoint = Body->GetSocketLocation(WeaponData->ThirdPersonAnchorBone) + Aim.RotateVector(Hold.Anchor);
	const FTransform Held(Rotation, GripPoint - Rotation.RotateVector(Grip * Scale), Scale);

	// While the hands are on it, remember where it is relative to the right hand; while an action has the hands, ride
	// that hand (before the first such moment: grip on the hand, barrel along the aim)
	const float HandsOn = BodyAnim ? BodyAnim->GetHandIKAlpha() : 0.0f;
	if (HandsOn > 0.99f)
	{
		ThirdPersonHandOffset = Held.GetRelativeTransform(HandTransform);
		bHasThirdPersonHandOffset = true;
	}
	FTransform OnHand = bHasThirdPersonHandOffset ? ThirdPersonHandOffset * HandTransform
		: FTransform(Rotation, HandTransform.GetLocation() - Rotation.RotateVector(Grip * Scale), Scale);

	// Better: put the rig's right palm (knuckles and their facing) exactly on the body's right palm, as the hand IK
	// does when aiming. Works before the hands were ever on it (a character that starts lowered).
	bool bPalmOnHand = false;
	{
		const USkeletalMesh* RigMesh = ThirdPersonMesh->GetSkeletalMeshAsset();
		if (RigMesh && ThirdPersonRigFingersMesh.Get() != RigMesh)
		{
			ThirdPersonRigFingersMesh = RigMesh;
			FTHHandShape::FindFingers(RigMesh->GetRefSkeleton(), WeaponData->ThirdPersonGripBone, ThirdPersonRigFingers[0]);
			FTHHandShape::FindFingers(RigMesh->GetRefSkeleton(), WeaponData->ThirdPersonLeftHandBone, ThirdPersonRigFingers[1]);
		}
		const USkeletalMesh* BodyMesh = Body->GetSkeletalMeshAsset();
		if (BodyMesh && ThirdPersonBodyFingersMesh.Get() != BodyMesh)
		{
			ThirdPersonBodyFingersMesh = BodyMesh;
			FTHHandShape::FindFingers(BodyMesh->GetRefSkeleton(), WeaponData->ThirdPersonHandBone, ThirdPersonBodyFingers);
		}
		const TArray<FName> (&Rig)[5] = ThirdPersonRigFingers[0];
		const TArray<FName> (&Own)[5] = ThirdPersonBodyFingers;
		if (WeaponData->bThirdPersonMatchRigHands && Rig[1].Num() && Rig[2].Num() && Rig[4].Num() && Own[1].Num() && Own[2].Num() && Own[4].Num())
		{
			auto RigCS = [this](FName Bone) { return ThirdPersonMesh->GetBoneLocation(Bone, EBoneSpaces::ComponentSpace); };
			const FVector RW = RigCS(WeaponData->ThirdPersonGripBone), RI = RigCS(Rig[1][0]), RM = RigCS(Rig[2][0]), RP = RigCS(Rig[4][0]);
			const FQuat RigFrame = FTHHandShape::MakeFrame(RW, RI, RM, RP);
			const FVector RigPalm = FTHHandShape::MakePalm(RW, RI, RM, RP);

			auto BodyWS = [Body](FName Bone) { return Body->GetSocketLocation(Bone); };
			const FVector BW = HandTransform.GetLocation(), BI = BodyWS(Own[1][0]), BM = BodyWS(Own[2][0]), BP = BodyWS(Own[4][0]);
			const FQuat BodyFrame = FTHHandShape::MakeFrame(BW, BI, BM, BP);
			const FVector BodyPalm = FTHHandShape::MakePalm(BW, BI, BM, BP);

			const FQuat GunRotation = (BodyFrame * RigFrame.Inverse()).GetNormalized();
			OnHand = FTransform(GunRotation, BodyPalm - GunRotation.RotateVector(RigPalm * Scale), Scale);
			bPalmOnHand = true;
		}
	}
	FTransform Placed = Held;
	if (HandsOn <= 0.99f)
	{
		Placed.Blend(OnHand, Held, HandsOn);
	}
	ThirdPersonMesh->SetWorldTransform(Placed);

	if (BodyAnim)
	{
		// Hand targets from the first-person rig's own hand bones, relative to the grip point, in aim space
		auto InAimSpace = [&](FName Bone)
		{
			return WeaponData->CameraHoldOffset.GetRotation().RotateVector((ThirdPersonMesh->GetBoneLocation(Bone, EBoneSpaces::ComponentSpace) - Grip) * Scale);
		};
		FTHWeaponGrip WeaponGrip;
		WeaponGrip.AnchorBone = WeaponData->ThirdPersonAnchorBone;
		WeaponGrip.AnchorOffset = Hold.Anchor;
		WeaponGrip.RotationOffset = WeaponData->ThirdPersonRotationOffset;
		WeaponGrip.RightHandTarget = Hold.RightOffset;
		WeaponGrip.RightHandRotation = Hold.RightRotation;
		WeaponGrip.LeftHandRotation = Hold.LeftRotation;
		WeaponGrip.bLeftHandFollowsRight = WeaponData->bThirdPersonLeftHandFollowsRight;
		WeaponGrip.RightShoulderOffset = Hold.RightShoulder;
		WeaponGrip.LeftShoulderOffset = Hold.LeftShoulder;

		// For the lowered carry: where the weapon sits on the right hand, and the rig's left palm in weapon space
		const TArray<FName> (&RigLeft)[5] = ThirdPersonRigFingers[1];
		if (bPalmOnHand && !WeaponData->ThirdPersonLeftHandBone.IsNone() && ThirdPersonMesh->GetBoneIndex(WeaponData->ThirdPersonLeftHandBone) != INDEX_NONE
			&& RigLeft[1].Num() && RigLeft[2].Num() && RigLeft[4].Num())
		{
			auto RigCS = [this](FName Bone) { return ThirdPersonMesh->GetBoneLocation(Bone, EBoneSpaces::ComponentSpace); };
			const FVector LW = RigCS(WeaponData->ThirdPersonLeftHandBone), LI = RigCS(RigLeft[1][0]), LM = RigCS(RigLeft[2][0]), LP = RigCS(RigLeft[4][0]);
			WeaponGrip.bHasGunFromRightHand = true;
			WeaponGrip.GunFromRightHand = OnHand.GetRelativeTransform(HandTransform);
			WeaponGrip.LeftPalmInGun = FTHHandShape::MakePalm(LW, LI, LM, LP);
			WeaponGrip.LeftFrameInGun = FTHHandShape::MakeFrame(LW, LI, LM, LP);
			WeaponGrip.HoldRotation = WeaponData->CameraHoldOffset.GetRotation();
		}

		// The rig's hands as they hold the weapon (palm, its facing, finger directions), for the body to copy
		const USkeletalMesh* RigMesh = ThirdPersonMesh->GetSkeletalMeshAsset();
		if (RigMesh && ThirdPersonRigFingersMesh.Get() != RigMesh)
		{
			ThirdPersonRigFingersMesh = RigMesh;
			FTHHandShape::FindFingers(RigMesh->GetRefSkeleton(), WeaponData->ThirdPersonGripBone, ThirdPersonRigFingers[0]);
			FTHHandShape::FindFingers(RigMesh->GetRefSkeleton(), WeaponData->ThirdPersonLeftHandBone, ThirdPersonRigFingers[1]);
		}
		auto MakeShape = [&](FName HandBone, const TArray<FName> (&Fingers)[FTHHandShape::NumFingers])
		{
			FTHHandShape Shape;
			if (!WeaponData->bThirdPersonMatchRigHands || HandBone.IsNone() || ThirdPersonMesh->GetBoneIndex(HandBone) == INDEX_NONE
				|| !Fingers[1].Num() || !Fingers[2].Num() || !Fingers[4].Num())
			{
				return Shape;
			}
			const FVector Wrist = InAimSpace(HandBone);
			const FVector Index = InAimSpace(Fingers[1][0]);
			const FVector Middle = InAimSpace(Fingers[2][0]);
			const FVector Pinky = InAimSpace(Fingers[4][0]);
			Shape.bValid = true;
			Shape.Frame = FTHHandShape::MakeFrame(Wrist, Index, Middle, Pinky);
			Shape.Palm = FTHHandShape::MakePalm(Wrist, Index, Middle, Pinky);
			for (int32 Finger = 0; Finger < FTHHandShape::NumFingers; ++Finger)
			{
				const TArray<FName>& Chain = Fingers[Finger];
				Shape.NumDirections[Finger] = FMath::Clamp(Chain.Num() - 1, 0, FTHHandShape::MaxJoints);
				for (int32 Joint = 0; Joint < Shape.NumDirections[Finger]; ++Joint)
				{
					Shape.FingerDirections[Finger][Joint] = (InAimSpace(Chain[Joint + 1]) - InAimSpace(Chain[Joint])).GetSafeNormal();
				}
			}
			return Shape;
		};
		WeaponGrip.RightShape = MakeShape(WeaponData->ThirdPersonGripBone, ThirdPersonRigFingers[0]);
		WeaponGrip.LeftShape = MakeShape(WeaponData->ThirdPersonLeftHandBone, ThirdPersonRigFingers[1]);
		if (WeaponGrip.bLeftHandFollowsRight)
		{
			WeaponGrip.bLeftHand = true;
			WeaponGrip.LeftHandTarget = Hold.LeftOffset;
		}
		else
		{
			WeaponGrip.bLeftHand = !WeaponData->ThirdPersonLeftHandBone.IsNone() && ThirdPersonMesh->GetBoneIndex(WeaponData->ThirdPersonLeftHandBone) != INDEX_NONE;
			// With a shape the palm already says where the hand goes, so the target is just the nudge
			if (WeaponGrip.bLeftHand)
			{
				WeaponGrip.LeftHandTarget = (WeaponGrip.LeftShape.bValid ? FVector::ZeroVector : InAimSpace(WeaponData->ThirdPersonLeftHandBone)) + Hold.LeftOffset;
			}
		}
		BodyAnim->SetWeaponGrip(WeaponGrip);
	}

	if (CVarDebugThirdPerson.GetValueOnGameThread() == 3)
	{
		ThirdPersonMesh->ShowAllMaterialSections(0);
	}
	if (CVarDebugThirdPerson.GetValueOnGameThread() == 2 && ThirdPersonMesh->GetMaterial(0) != UMaterial::GetDefaultMaterial(MD_Surface))
	{
		for (int32 Index = 0; Index < ThirdPersonMesh->GetNumMaterials(); ++Index)
		{
			ThirdPersonMesh->SetMaterial(Index, UMaterial::GetDefaultMaterial(MD_Surface));
		}
	}
	if (CVarDebugThirdPerson.GetValueOnGameThread())
	{
		const FVector Muzzle = ThirdPersonMesh->GetSocketLocation(WeaponData->MuzzleSocketName);
		const FVector GripWorld = ThirdPersonMesh->GetBoneLocation(WeaponData->ThirdPersonGripBone);
		const FVector Root = ThirdPersonMesh->GetBoneLocation(ThirdPersonMesh->GetBoneName(0));
		DrawDebugLine(GetWorld(), GripWorld, Muzzle, FColor::Red, false, -1.0f, SDPG_Foreground, 0.5f);
		DrawDebugLine(GetWorld(), GripWorld, GripWorld + Aim.Vector() * 60.0f, FColor::Green, false, -1.0f, SDPG_Foreground, 0.3f);
		DrawDebugBox(GetWorld(), ThirdPersonMesh->Bounds.Origin, ThirdPersonMesh->Bounds.BoxExtent, FColor::Yellow, false, -1.0f, SDPG_Foreground, 0.3f);
		UE_LOG(LogTacticalHorror, Warning, TEXT("TP actor=%s body=%s"), *Pawn->GetActorRotation().ToString(), *Body->GetComponentRotation().ToString());
		const FQuat TPBone = ThirdPersonMesh->GetSocketQuaternion(WeaponData->MuzzleSocketName);
		const FQuat FPBone = Mesh->GetSocketQuaternion(WeaponData->MuzzleSocketName);
		const FQuat FPParent = Mesh->GetAttachParent() ? Mesh->GetAttachParent()->GetComponentQuat() : FQuat::Identity;
		UE_LOG(LogTacticalHorror, Warning, TEXT("TP axes: aimFwd=%s | TP boneX=%s Y=%s Z=%s | FP boneX=%s Y=%s Z=%s | FP parent fwd=%s parent=%s"),
			*Aim.GetForwardVector().ToString(), *TPBone.GetAxisX().ToString(), *TPBone.GetAxisY().ToString(), *TPBone.GetAxisZ().ToString(),
			*FPBone.GetAxisX().ToString(), *FPBone.GetAxisY().ToString(), *FPBone.GetAxisZ().ToString(), *FPParent.GetForwardVector().ToString(),
			*GetNameSafe(Mesh->GetAttachParent()));
		UE_LOG(LogTacticalHorror, Warning, TEXT("TP %s: aim=%s hand=%s gripCS=%s gripWorld=%s muzzle=%s barrelDir=%s root=%s bones=%d anim=%s"),
			*GetName(), *Aim.Rotator().ToString(), *HandTransform.GetLocation().ToString(), *Grip.ToString(), *GripWorld.ToString(), *Muzzle.ToString(),
			*(Muzzle - GripWorld).GetSafeNormal().ToString(), *Root.ToString(), ThirdPersonMesh->GetNumBones(),
			*GetNameSafe(ThirdPersonMesh->GetSingleNodeInstance() ? ThirdPersonMesh->GetSingleNodeInstance()->GetAnimationAsset() : nullptr));
	}
}

FRotator ATHWeaponBase::GetPresentationAim(const APawn* Pawn)
{
	// Pitch from the aim, yaw from the body. Same as the aim in normal play (the body turns with the view; for other
	// players' characters the aim yaw is the body's anyway), and stays right when watching from a debug camera.
	return FRotator(Pawn->GetBaseAimRotation().Pitch, Pawn->GetActorRotation().Yaw, 0.0f);
}

USkeletalMeshComponent* ATHWeaponBase::GetVisibleMesh() const
{
	if (!ThirdPersonBody.IsValid())
	{
		return Mesh;
	}

	// The owner sees first person while looking through its own eyes (not, say, from a debug camera)
	const APawn* Pawn = GetOwningPawn();
	const APlayerController* PC = Pawn && Pawn->IsLocallyControlled() ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return PC && PC->GetViewTarget() == Pawn ? Mesh.Get() : ThirdPersonMesh.Get();
}

void ATHWeaponBase::PlayBodyAction(UAnimSequence* Animation, float Duration, float BlendIn, float BlendOut)
{
	const APawn* Pawn = GetOwningPawn();
	const UTHWeaponComponent* Weapons = Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	if (UTHBodyAnimInstance* Body = Animation && Weapons ? Weapons->GetBodyAnimInstance() : nullptr)
	{
		Body->PlayAction(Animation, Duration, BlendIn, BlendOut);
	}
}

void ATHWeaponBase::UpdateFirstPersonRendering()
{
	// The weapon materials have a first-person output (UE 5.7 first-person rendering). For the local player the mesh
	// must be flagged as a first-person primitive, otherwise that output distorts it across the whole screen.
	const bool bFirstPerson = IsLocallyControlledOwner() && GetVisibleMesh() == Mesh;
	const EFirstPersonPrimitiveType Desired = bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None;
	if (Mesh->FirstPersonPrimitiveType != Desired)
	{
		Mesh->SetFirstPersonPrimitiveType(Desired);
		for (UStaticMeshComponent* Quad : MuzzleFlashQuads)
		{
			Quad->SetFirstPersonPrimitiveType(Desired);
		}
		WeaponLightMesh->SetFirstPersonPrimitiveType(Desired);
	}
}

bool ATHWeaponBase::CanFire() const
{
	return WeaponData && bIsEquipped && !bFiringBlocked && !bIsReloading
		&& GetWorld()->GetTimeSeconds() >= EquipFinishTime;
}

void ATHWeaponBase::HandleFiring()
{
	if (!bWantsToFire || !CanFire())
	{
		return;
	}

	if (GetFireMode() == ETHFireMode::SemiAuto && bFiredThisTriggerPull)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextFireTime)
	{
		return;
	}

	if (CurrentAmmo <= 0)
	{
		bFiredThisTriggerPull = true;
		bWantsToFire = false;
		if (WeaponData->DryFireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, WeaponData->DryFireSound, GetActorLocation(), UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this));
		}
		StartReload();
		return;
	}

	// Whether this shot continues a held trigger (full auto), before this shot marks the pull as fired
	const bool bContinuingBurst = bFiredThisTriggerPull;

	FireShot();
	bFiredThisTriggerPull = true;

	// While the trigger is held, keep a steady cadence when the frame lands slightly late (but don't bank shots after
	// a pause). A new trigger pull always waits a full interval from this shot: otherwise a click just after the last
	// one was due could fire almost immediately, faster than full auto.
	const float Interval = WeaponData->GetFireInterval();
	NextFireTime = (bContinuingBurst && Now - NextFireTime < Interval) ? NextFireTime + Interval : Now + Interval;
}

void ATHWeaponBase::FireShot()
{
	APawn* Pawn = GetOwningPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	LocalSprayIndex = GetSprayIndexForShot(LocalSprayIndex, LastLocalShotTime, Now);
	LastLocalShotTime = Now;
	++LocalShotNumber;

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const int32 SprayIndex = LocalSprayIndex;
	const int32 ShotNumber = LocalShotNumber;

	if (HasAuthority())
	{
		// Listen-server host: authoritative shot, effects play through the multicast-skip path below
		ProcessServerShot(ViewLocation, ViewRotation, SprayIndex, ShotNumber);
	}
	else
	{
		// Predict ammo and the impact locally so the shot feels instant; the server corrects ammo if it disagrees
		--CurrentAmmo;

		FHitResult Hit;
		const FVector Direction = ComputeShotDirection(ViewRotation, SprayIndex, ShotNumber);
		if (TraceShot(ViewLocation, Direction, Hit))
		{
			PlayImpactEffects(Hit.ImpactPoint, Hit.ImpactNormal, UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()));
		}

		ServerFire(ViewLocation, ViewRotation, SprayIndex, ShotNumber);
	}

	PlayFireEffects();
	ApplyViewKick(SprayIndex);
	++LocalSprayIndex;
}

int32 ATHWeaponBase::GetSprayIndexForShot(int32 PreviousIndex, float LastShotTime, float Now) const
{
	return (Now - LastShotTime > WeaponData->RecoilResetDelay) ? 0 : PreviousIndex;
}

float ATHWeaponBase::GetCurrentInaccuracy() const
{
	if (!WeaponData)
	{
		return 0.0f;
	}

	// Spray index of the shot that would be fired now
	const bool bLocal = IsLocallyControlledOwner();
	const float LastShot = bLocal ? LastLocalShotTime : LastServerShotTime;
	const int32 PrevIndex = bLocal ? LocalSprayIndex : ServerSprayIndex;
	return GetInaccuracyForSprayIndex(GetSprayIndexForShot(PrevIndex, LastShot, GetWorld()->GetTimeSeconds()));
}

float ATHWeaponBase::GetInaccuracyForSprayIndex(int32 SprayIndex) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!WeaponData || !Character)
	{
		return 0.0f;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float SpeedAlpha = Movement && Movement->MaxWalkSpeed > 0.0f
		? FMath::Clamp(Character->GetVelocity().Size2D() / Movement->MaxWalkSpeed, 0.0f, 1.0f)
		: 0.0f;
	const bool bFalling = Movement && Movement->IsFalling();

	if (WeaponData->bFirstShotAccurate && SprayIndex == 0 && !bFalling && SpeedAlpha < 0.1f)
	{
		return 0.0f;
	}

	float Inaccuracy = bFalling
		? WeaponData->AirInaccuracy
		: FMath::Lerp(WeaponData->StandingInaccuracy, WeaponData->MovingInaccuracy, SpeedAlpha);

	Inaccuracy += FMath::Min(SprayIndex * WeaponData->InaccuracyPerShot, WeaponData->MaxSprayInaccuracy);

	if (Character->bIsCrouched)
	{
		Inaccuracy *= WeaponData->CrouchInaccuracyMultiplier;
	}
	if (IsAimingDownSights())
	{
		Inaccuracy *= WeaponData->AimInaccuracyMultiplier;
	}

	return Inaccuracy;
}

FVector ATHWeaponBase::ComputeShotDirection(const FRotator& ViewRotation, int32 SprayIndex, int32 ShotNumber) const
{
	// The camera already carries the view-kick share of the recoil, so bullets only add the remainder
	const FVector2D Recoil = WeaponData->GetCumulativeRecoil(SprayIndex) * (1.0f - WeaponData->ViewKickFraction);

	FRotator AimRotation = ViewRotation;
	AimRotation.Pitch += Recoil.Y;
	AimRotation.Yaw += Recoil.X;

	const float Inaccuracy = GetInaccuracyForSprayIndex(SprayIndex);
	if (Inaccuracy <= KINDA_SMALL_NUMBER)
	{
		return AimRotation.Vector();
	}

	FRandomStream Stream(HashCombine(static_cast<uint32>(SpreadSeed), static_cast<uint32>(ShotNumber)));
	return Stream.VRandCone(AimRotation.Vector(), FMath::DegreesToRadians(Inaccuracy));
}

bool ATHWeaponBase::TraceShot(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THWeaponTrace), false, this);
	Params.AddIgnoredActor(GetOwner());
	Params.bReturnPhysicalMaterial = true;

	// Bullets stop on visible geometry only: invisible shapes (pickup and interaction spheres, triggers) often block every
	// channel, so step through them. Pawn capsules already ignore the Weapon channel, letting traces reach their bones.
	const FVector End = Start + Direction * WeaponData->MaxRange;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		if (!GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params))
		{
			return false;
		}
		UShapeComponent* Shape = Cast<UShapeComponent>(OutHit.GetComponent());
		if (!Shape)
		{
			return true;
		}
		Params.AddIgnoredComponent(Shape);
	}
	return false;
}

// Recoil (owning client)

void ATHWeaponBase::ApplyViewKick(int32 SprayIndex)
{
	APawn* Pawn = GetOwningPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller || WeaponData->RecoilPattern.IsEmpty())
	{
		return;
	}

	const FVector2D Step = WeaponData->RecoilPattern[FMath::Min(SprayIndex, WeaponData->RecoilPattern.Num() - 1)]
		* WeaponData->RecoilScale * WeaponData->ViewKickFraction;

	// Set the rotation directly so recoil is in exact degrees, independent of input scaling
	Controller->SetControlRotation(Controller->GetControlRotation() + FRotator(Step.Y, Step.X, 0.0f));
	PendingViewRecovery += Step;
}

void ATHWeaponBase::UpdateViewRecovery(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bStillSpraying = Now - LastLocalShotTime < WeaponData->GetFireInterval() * 1.5f;
	if (bStillSpraying || PendingViewRecovery.IsNearlyZero())
	{
		return;
	}

	APawn* Pawn = GetOwningPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	const float Remaining = PendingViewRecovery.Size();
	const float StepSize = FMath::Min(Remaining, WeaponData->ViewRecoveryRate * DeltaSeconds);
	const FVector2D Step = PendingViewRecovery / Remaining * StepSize;

	Controller->SetControlRotation(Controller->GetControlRotation() - FRotator(Step.Y, Step.X, 0.0f));
	PendingViewRecovery -= Step;
}

// Server

void ATHWeaponBase::ServerFire_Implementation(FVector_NetQuantize10 ViewLocation, FRotator ViewRotation, int32 ClientSprayIndex, int32 ShotNumber)
{
	ProcessServerShot(ViewLocation, ViewRotation, ClientSprayIndex, ShotNumber);
}

void ATHWeaponBase::ProcessServerShot(const FVector& ViewLocation, const FRotator& ViewRotation, int32 ClientSprayIndex, int32 ShotNumber)
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !WeaponData || !bIsEquipped || bFiringBlocked || bIsReloading || CurrentAmmo <= 0)
	{
		return;
	}

	// Each shot number can only be used once, which stops replaying a favourable spread seed
	if (ShotNumber <= LastServerShotNumber)
	{
		return;
	}

	// Allow some jitter in arrival times, but not a faster fire rate
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerShotTime < WeaponData->GetFireInterval() * 0.75f)
	{
		return;
	}

	ServerSprayIndex = GetSprayIndexForShot(ServerSprayIndex, LastServerShotTime, Now);
	const int32 SprayIndex = FMath::Abs(ClientSprayIndex - ServerSprayIndex) <= 2 ? ClientSprayIndex : ServerSprayIndex;

	FVector Origin = ViewLocation;
	if (FVector::DistSquared(Origin, Pawn->GetPawnViewLocation()) > FMath::Square(MaxOriginError))
	{
		Origin = Pawn->GetPawnViewLocation();
	}

	LastServerShotNumber = ShotNumber;
	LastServerShotTime = Now;
	ServerSprayIndex = SprayIndex + 1;
	--CurrentAmmo;

	FHitResult Hit;
	const FVector Direction = ComputeShotDirection(ViewRotation, SprayIndex, ShotNumber);
	const bool bHit = TraceShot(Origin, Direction, Hit);

	if (bHit && Hit.GetActor())
	{
		float Damage = WeaponData->Damage * FMath::Pow(WeaponData->RangeModifierPer10m, Hit.Distance / 1000.0f);
		if (WeaponData->HeadBoneNames.Contains(Hit.BoneName))
		{
			Damage *= WeaponData->HeadshotMultiplier;
		}

		const TSubclassOf<UDamageType> DamageType = WeaponData->DamageType ? WeaponData->DamageType : TSubclassOf<UDamageType>(UDamageType::StaticClass());
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Direction, Hit, Pawn->GetController(), this, DamageType);
	}

	const FVector ImpactPoint = bHit ? Hit.ImpactPoint : Origin + Direction * WeaponData->MaxRange;
	const uint8 Surface = bHit ? (uint8)UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()) : (uint8)SurfaceType_Default;
	MulticastShotEffects(ImpactPoint, Hit.ImpactNormal, bHit, Surface);
}

void ATHWeaponBase::MulticastShotEffects_Implementation(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal, bool bHit, uint8 Surface)
{
	// The shooter already played these when firing
	if (IsLocallyControlledOwner() || GetNetMode() == NM_DedicatedServer)
	{
		// Except the listen-server host's own impact, which wasn't predicted separately
		if (HasAuthority() && IsLocallyControlledOwner() && bHit)
		{
			PlayImpactEffects(ImpactPoint, ImpactNormal, Surface);
		}
		return;
	}

	PlayFireEffects();
	if (bHit)
	{
		PlayImpactEffects(ImpactPoint, ImpactNormal, Surface);
	}
}

// Reload

void ATHWeaponBase::ServerReload_Implementation()
{
	if (!WeaponData || !bIsEquipped || bIsReloading || CurrentAmmo >= WeaponData->MagazineSize || ReserveAmmo <= 0)
	{
		return;
	}

	bIsReloading = true;
	OnRep_IsReloading();
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ATHWeaponBase::FinishReload, FMath::Max(WeaponData->ReloadDuration, 0.01f), false);
}

void ATHWeaponBase::FinishReload()
{
	const int32 Needed = WeaponData->MagazineSize - CurrentAmmo;
	const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
	CurrentAmmo += Loaded;
	ReserveAmmo -= Loaded;

	bIsReloading = false;
	OnRep_IsReloading();
}

void ATHWeaponBase::OnRep_IsReloading()
{
	if (bIsReloading)
	{
		// Third person: the magazine comes out and a new one goes in, timed from here
		ResetThirdPersonMagazine();
		ReloadStartTime = GetWorld()->GetTimeSeconds();

		if (WeaponData)
		{
			if (WeaponData->ReloadSound)
			{
				UGameplayStatics::SpawnSoundAttached(WeaponData->ReloadSound, Mesh, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
					false, UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this));
			}

			// Ammo only replicates to the owner, so other players always see the regular reload
			const bool bEmpty = CurrentAmmo <= 0 && WeaponData->ReloadEmptyAnimation;
			PlayWeaponAnimation(bEmpty ? WeaponData->ReloadEmptyAnimation : WeaponData->ReloadAnimation);

			if (const UTHBodyAnimSet* BodySet = WeaponData->BodyAnimSet)
			{
				PlayBodyAction(BodySet->Reload, WeaponData->ReloadDuration, BodySet->ActionBlendTime, FMath::Max(BodySet->ActionBlendTime, BodySet->ReloadBlendOutTime));
			}
		}
		ReceiveReloadStarted();
	}
	else
	{
		ReceiveReloadFinished();
	}
}

// Effects

void ATHWeaponBase::PlayFireEffects()
{
	if (!WeaponData)
	{
		return;
	}

	const FTransform Muzzle = GetMuzzleTransform();

	const int32 NumFireSounds = WeaponData->FireSounds.Num();
	if (NumFireSounds > 0)
	{
		int32 Index = FMath::RandRange(0, NumFireSounds - 1);
		if (NumFireSounds > 1 && Index == LastFireSoundIndex)
		{
			Index = (Index + 1) % NumFireSounds;
		}
		LastFireSoundIndex = Index;

		if (UAudioComponent* Previous = LastFireAudio.Get(); Previous && WeaponData->FireSoundTailFadeOut > 0.0f)
		{
			Previous->FadeOut(WeaponData->FireSoundTailFadeOut, 0.0f);
		}

		const FVector2D& Pitch = WeaponData->FireSoundPitchRange;
		LastFireAudio = UGameplayStatics::SpawnSoundAtLocation(this, WeaponData->FireSounds[Index], Muzzle.GetLocation(), FRotator::ZeroRotator,
			UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this), FMath::FRandRange(Pitch.X, Pitch.Y), 0.0f, nullptr, WeaponData->FireSoundConcurrency);
	}

	if (WeaponData->MuzzleFlash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->MuzzleFlash, Muzzle.GetLocation(), Muzzle.Rotator());
	}

	ShowMuzzleFlash(Muzzle);
	EjectShellCasing(Muzzle);

	PlayWeaponAnimation(WeaponData->FireAnimation);

	if (const UTHBodyAnimSet* BodySet = WeaponData->BodyAnimSet)
	{
		PlayBodyAction(BodySet->Fire, 0.0f, 0.0f, 0.1f);
	}
	// Shooting brings the weapon up from a lowered carry (and keeps it up for the set's Ready Hold Time)
	if (const UTHWeaponComponent* Weapons = GetOwningPawn() ? GetOwningPawn()->FindComponentByClass<UTHWeaponComponent>() : nullptr)
	{
		if (UTHBodyAnimInstance* Body = Weapons->GetBodyAnimInstance())
		{
			Body->RaiseWeapon();
		}
	}

	if (IsLocallyControlledOwner() && WeaponData->FireCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetOwningPawn()->GetController()))
		{
			PC->ClientStartCameraShake(WeaponData->FireCameraShake);
		}
	}

	ReceiveFired();
}

void ATHWeaponBase::ShowMuzzleFlash(const FTransform& Muzzle)
{
	// Player comfort setting: the flash can be harsh in dark rooms. Purely cosmetic and local to each player.
	struct FFlashQuality { float Brightness; float Size; float Light; };
	static const FFlashQuality Qualities[] = { { 0.25f, 0.7f, 0.2f }, { 0.55f, 0.85f, 0.5f }, { 1.0f, 1.0f, 1.0f } };
	const FFlashQuality& Quality = Qualities[FMath::Clamp(CVarMuzzleFlashQuality.GetValueOnGameThread(), 0, 2)];

	const bool bLight = WeaponData->MuzzleLightIntensity > 0.0f;
	const bool bQuads = WeaponData->MuzzleFlashMaterial && WeaponData->MuzzleFlashSize > 0.0f;
	if (!bLight && !bQuads)
	{
		return;
	}

	if (bLight)
	{
		MuzzleLight->SetWorldLocation(Muzzle.GetLocation());
		MuzzleLight->SetLightColor(WeaponData->MuzzleLightColor);
		MuzzleLight->SetIntensity(WeaponData->MuzzleLightIntensity * Quality.Light);
		MuzzleLight->SetVisibility(true);
	}

	if (bQuads)
	{
		// The pawn's aim is the barrel direction on every machine; muzzle bones are not authored consistently between packs
		const APawn* Pawn = GetOwningPawn();
		const FVector Aim = Pawn ? GetPresentationAim(Pawn).Vector() : Muzzle.GetRotation().GetForwardVector();
		const float Size = WeaponData->MuzzleFlashSize * Quality.Size * FMath::FRandRange(0.8f, 1.2f);

		if (!MuzzleFlashMaterial || MuzzleFlashMaterial->Parent != WeaponData->MuzzleFlashMaterial)
		{
			MuzzleFlashMaterial = UMaterialInstanceDynamic::Create(WeaponData->MuzzleFlashMaterial, this);
		}
		float BaseIntensity = 1.0f;
		WeaponData->MuzzleFlashMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Intensity")), BaseIntensity);
		MuzzleFlashMaterial->SetScalarParameterValue(TEXT("Intensity"), BaseIntensity * Quality.Brightness);
		const float Roll = FMath::FRandRange(0.0f, 360.0f);

		// The engine plane is 100 cm square in XY, facing +Z
		FRotator Facing = FRotationMatrix::MakeFromZ(Aim).Rotator();
		const FQuat Spin(Aim, FMath::DegreesToRadians(Roll));
		MuzzleFlashQuads[0]->SetWorldTransform(FTransform(Spin * Facing.Quaternion(), Muzzle.GetLocation() + Aim * Size * 0.3f, FVector(Size / 100.0f)));

		const float Length = Size * 1.6f;
		for (int32 Index = 1; Index < MuzzleFlashQuads.Num(); ++Index)
		{
			const FQuat Along = FQuat(Aim, FMath::DegreesToRadians(Roll + 90.0f * Index)) * FRotationMatrix::MakeFromX(Aim).ToQuat();
			MuzzleFlashQuads[Index]->SetWorldTransform(FTransform(Along, Muzzle.GetLocation() + Aim * Length * 0.5f, FVector(Length / 100.0f, Size * 0.6f / 100.0f, 1.0f)));
		}

		for (UStaticMeshComponent* Quad : MuzzleFlashQuads)
		{
			Quad->SetMaterial(0, MuzzleFlashMaterial);
			Quad->SetVisibility(true);
		}
	}

	GetWorldTimerManager().SetTimer(MuzzleFlashTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		MuzzleLight->SetVisibility(false);
		for (UStaticMeshComponent* Quad : MuzzleFlashQuads)
		{
			Quad->SetVisibility(false);
		}
	}), FMath::Max(WeaponData->MuzzleLightDuration, 0.01f), false);
}

void ATHWeaponBase::EjectShellCasing(const FTransform& Muzzle)
{
	if (!WeaponData->ShellMesh)
	{
		return;
	}

	// Aim space like the muzzle flash: the pawn's aim is the barrel direction on every machine
	const APawn* Pawn = GetOwningPawn();
	const FQuat Aim = Pawn ? GetPresentationAim(Pawn).Quaternion() : Muzzle.GetRotation();
	const FVector Location = Muzzle.GetLocation() + Aim.RotateVector(WeaponData->ShellEjectOffset);

	const FVector& Base = WeaponData->ShellEjectVelocity;
	const float Jitter = Base.Size() * 0.12f;
	const FVector LocalVelocity = Base * FMath::FRandRange(0.85f, 1.15f)
		+ FVector(FMath::FRandRange(-Jitter, Jitter), 0.0f, FMath::FRandRange(-Jitter, Jitter));
	const FVector Velocity = Aim.RotateVector(LocalVelocity) + (Pawn ? Pawn->GetVelocity() : FVector::ZeroVector);

	// Mostly an end-over-end flip about the vertical, with some wobble
	const FVector Spin = Aim.RotateVector(FVector(FMath::FRandRange(-200.0f, 200.0f), FMath::FRandRange(-200.0f, 200.0f), FMath::FRandRange(700.0f, 1400.0f)));

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FQuat Rotation = Aim * WeaponData->ShellMeshRotation.Quaternion();
	ATHShellCasing* Shell = GetWorld()->SpawnActor<ATHShellCasing>(Location, Rotation.Rotator(), Params);
	if (!Shell)
	{
		return;
	}
	Shell->Launch(WeaponData, Velocity, Spin);

	ShellCasings.RemoveAll([](const TWeakObjectPtr<ATHShellCasing>& Ptr) { return !Ptr.IsValid(); });
	ShellCasings.Add(Shell);
	while (ShellCasings.Num() > WeaponData->MaxShellCasings)
	{
		if (ATHShellCasing* Oldest = ShellCasings[0].Get())
		{
			Oldest->Destroy();
		}
		ShellCasings.RemoveAt(0);
	}
}

FTransform ATHWeaponBase::GetMuzzleTransform() const
{
	// Effects come from the copy this machine sees: the first-person gun for its owner, the one in the hand for others
	const USkeletalMeshComponent* Visible = GetVisibleMesh();
	if (!WeaponData)
	{
		return Visible->GetComponentTransform();
	}

	// GetSocketTransform also accepts bone names, and falls back to the component for unknown names
	const FTransform Base = Visible->GetSocketTransform(WeaponData->MuzzleSocketName);
	return FTransform(Base.GetRotation(), Base.TransformPosition(WeaponData->MuzzleOffset));
}

void ATHWeaponBase::PlayWeaponAnimation(UAnimSequenceBase* Animation)
{
	// Single-node playback: enough for a viewmodel that only needs one animation at a time.
	// Switch to an Anim Blueprint with montages when blending (idle sway, ADS poses) is needed.
	if (!Animation)
	{
		return;
	}

	Mesh->PlayAnimation(Animation, false);

	// Return to the idle loop when this one finishes (a newer animation restarts the timer)
	UAnimSequenceBase* Idle = WeaponData ? WeaponData->IdleAnimation.Get() : nullptr;
	if (Idle && Idle != Animation)
	{
		GetWorldTimerManager().SetTimer(IdleAnimTimer, FTimerDelegate::CreateWeakLambda(this, [this, Idle]()
		{
			Mesh->PlayAnimation(Idle, true);
		}), Animation->GetPlayLength(), false);
	}
}

void ATHWeaponBase::PlayImpactEffects(const FVector& Location, const FVector& Normal, uint8 Surface)
{
	if (!WeaponData)
	{
		return;
	}

	if (WeaponData->ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->ImpactEffect, Location, Normal.Rotation());
	}

	UMaterialInterface* DecalMaterial = WeaponData->ImpactSurfaces
		? WeaponData->ImpactSurfaces->PickDecal((EPhysicalSurface)Surface) : WeaponData->ImpactDecal.Get();
	if (DecalMaterial && WeaponData->ImpactDecalSize > 0.0f)
	{
		// Decals project along their X axis: into the surface, spun randomly so holes don't repeat
		FRotator Rotation = (-Normal).Rotation();
		Rotation.Roll = FMath::FRandRange(0.0f, 360.0f);
		const float HalfSize = WeaponData->ImpactDecalSize * 0.5f;
		const FVector DecalSize(2.0f, HalfSize, HalfSize);
		// A hole in something that moves (a range target on its carrier) goes with it. The shot only brings the point, so
		// find what is there on this machine.
		FHitResult SurfaceHit;
		const bool bFound = GetWorld()->LineTraceSingleByChannel(SurfaceHit, Location + Normal * 2.0f, Location - Normal * 2.0f,
			TraceChannel, FCollisionQueryParams(SCENE_QUERY_STAT(THImpactSurface), false, GetOwner()));
		USceneComponent* MovingSurface = bFound && SurfaceHit.GetComponent() && SurfaceHit.GetComponent()->Mobility == EComponentMobility::Movable
			? SurfaceHit.GetComponent() : nullptr;
		UDecalComponent* Decal = MovingSurface
			? UGameplayStatics::SpawnDecalAttached(DecalMaterial, DecalSize, MovingSurface, NAME_None, Location, Rotation,
				EAttachLocation::KeepWorldPosition, WeaponData->ImpactDecalLifetime)
			: UGameplayStatics::SpawnDecalAtLocation(this, DecalMaterial, DecalSize, Location, Rotation, WeaponData->ImpactDecalLifetime);
		if (Decal)
		{
			Decal->SetFadeScreenSize(0.0f);
		}
	}

	if (WeaponData->ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->ImpactSound, Location, UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this));
	}
}

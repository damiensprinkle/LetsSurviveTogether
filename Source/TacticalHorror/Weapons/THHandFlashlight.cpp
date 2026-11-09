#include "Weapons/THHandFlashlight.h"
#include "Weapons/THWeaponComponent.h"
#include "Weapons/THWeaponData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATHHandFlashlight::ATHHandFlashlight()
{
	static ConstructorHelpers::FObjectFinder<UTHWeaponData> DefaultData(TEXT("/Game/Weapons/Data/DA_HandFlashlight.DA_HandFlashlight"));
	WeaponData = DefaultData.Object;
	// Paths only (loaded when spawned): assets the class default holds can't be moved or renamed in the editor
	ArmsMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/ThirdParty/FPFlashlightAnims/Demo/Player/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms")));
	FlashlightMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ThirdParty/FPFlashlightAnims/Mesh/SM_Flashlight.SM_Flashlight")));
	InspectAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Weapons/HandFlashlight/Animations/A_FP_HandFlashlight_Inspect.A_FP_HandFlashlight_Inspect")));

	HeldFlashlight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldFlashlight"));
	HeldFlashlight->SetupAttachment(Mesh, HandSocket);
	HeldFlashlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldFlashlight->SetCastShadow(false);
	HeldFlashlight->SetOnlyOwnerSee(true);

	ThirdPersonFlashlight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonFlashlight"));
	ThirdPersonFlashlight->SetupAttachment(Mesh);
	ThirdPersonFlashlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonFlashlight->SetOwnerNoSee(true);
	ThirdPersonFlashlight->SetHiddenInGame(true);

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam"));
	Beam->SetupAttachment(HeldFlashlight);
	Beam->SetVisibility(false);
}

void ATHHandFlashlight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATHHandFlashlight, bLightOn, COND_SkipOwner);
}

void ATHHandFlashlight::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!Mesh->GetSkeletalMeshAsset())
	{
		Mesh->SetSkeletalMeshAsset(ArmsMesh.LoadSynchronous());
	}
}

void ATHHandFlashlight::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Model = FlashlightMesh.LoadSynchronous();
	HeldFlashlight->SetStaticMesh(Model);
	ThirdPersonFlashlight->SetStaticMesh(Model);
	Beam->SetIntensity(Intensity);
	Beam->SetAttenuationRadius(Range);
	Beam->SetInnerConeAngle(InnerCone);
	Beam->SetOuterConeAngle(OuterCone);
	Beam->SetLightColor(Color);
}

void ATHHandFlashlight::OnEquipped()
{
	Super::OnEquipped();
	AttachThirdPersonFlashlight();
	UpdateBeam();
}

void ATHHandFlashlight::OnUnequipped()
{
	Super::OnUnequipped();
	ThirdPersonFlashlight->SetHiddenInGame(true);
	ThirdPersonFlashlight->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Beam->SetVisibility(false);
}

void ATHHandFlashlight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The owner's model is drawn like the first-person arms holding it
	if (HeldFlashlight->FirstPersonPrimitiveType != Mesh->FirstPersonPrimitiveType)
	{
		HeldFlashlight->SetFirstPersonPrimitiveType(Mesh->FirstPersonPrimitiveType);
	}
	if (IsEquipped() && !ThirdPersonFlashlight->GetAttachParent()->IsA<USkeletalMeshComponent>())
	{
		AttachThirdPersonFlashlight();
	}
	// Others see it appear once the body's draw has brought the hand up
	if (IsEquipped() && ThirdPersonFlashlight->GetAttachParent()->IsA<USkeletalMeshComponent>())
	{
		ThirdPersonFlashlight->SetHiddenInGame(IsThirdPersonHiddenForDraw());
	}
	UpdateBeam();
}

void ATHHandFlashlight::AttachThirdPersonFlashlight()
{
	const APawn* Pawn = GetOwningPawn();
	const UTHWeaponComponent* Weapons = Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	USkeletalMeshComponent* Body = Weapons ? Weapons->GetBodyMesh() : nullptr;
	if (!Body)
	{
		return;
	}

	// The arms' socket sits on their hand_r; the body's hand_r is oriented the same way (both UE5 mannequins)
	FTransform SocketOffset = FTransform::Identity;
	if (const USkeletalMesh* Arms = Mesh->GetSkeletalMeshAsset())
	{
		if (const USkeletalMeshSocket* Socket = Arms->FindSocket(HandSocket))
		{
			SocketOffset = Socket->GetSocketLocalTransform();
		}
	}
	ThirdPersonFlashlight->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ThirdPersonHandBone);
	ThirdPersonFlashlight->SetRelativeTransform(ThirdPersonAdjust * SocketOffset);
	ThirdPersonFlashlight->SetHiddenInGame(IsThirdPersonHiddenForDraw());
}

void ATHHandFlashlight::UpdateBeam()
{
	// Lights reach every viewer, so one beam per machine: from the model this machine sees
	UStaticMeshComponent* Source = GetVisibleMesh() == Mesh ? HeldFlashlight.Get() : ThirdPersonFlashlight.Get();
	if (Beam->GetAttachParent() != Source)
	{
		Beam->AttachToComponent(Source, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Beam->SetRelativeTransform(BeamOffset);
	Beam->SetVisibility(IsEquipped() && bLightOn && !(Source == ThirdPersonFlashlight && ThirdPersonFlashlight->bHiddenInGame));
}

void ATHHandFlashlight::StartFire()
{
	if (!IsEquipped())
	{
		return;
	}
	bLightOn = !bLightOn;
	if (!HasAuthority())
	{
		ServerSetLightOn(bLightOn);
	}
	if (USoundBase* Click = GetLightClickSound())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Click, GetActorLocation());
	}
	UpdateBeam();
}

void ATHHandFlashlight::ServerSetLightOn_Implementation(bool bOn)
{
	bLightOn = bOn;
}

void ATHHandFlashlight::PlayInspect()
{
	PlayWeaponAnimation(InspectAnimation.LoadSynchronous());
}

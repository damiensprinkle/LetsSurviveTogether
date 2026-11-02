#include "Weapons/THWeaponComponent.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponData.h"
#include "Player/THBodyAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

UTHWeaponComponent::UTHWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	BodyAnimClass = UTHBodyAnimInstance::StaticClass();
}

USkeletalMeshComponent* UTHWeaponComponent::GetBodyMesh() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return nullptr;
	}
	if (USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh && Mesh->GetSkeletalMeshAsset())
	{
		return Mesh;
	}

	// The template character's body is an extra component; ACharacter's own Mesh is left empty
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Character);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetSkeletalMeshAsset())
		{
			return Mesh;
		}
	}
	return nullptr;
}

UTHBodyAnimInstance* UTHWeaponComponent::GetBodyAnimInstance() const
{
	const USkeletalMeshComponent* Body = GetBodyMesh();
	return Body ? Cast<UTHBodyAnimInstance>(Body->GetAnimInstance()) : nullptr;
}

UTHBodyAnimSet* UTHWeaponComponent::GetHeldBodyAnimSet() const
{
	const UTHWeaponData* Data = CurrentWeapon ? CurrentWeapon->GetWeaponData() : nullptr;
	return Data ? Data->BodyAnimSet.Get() : nullptr;
}

void UTHWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTHWeaponComponent, Weapons);
	DOREPLIFETIME(UTHWeaponComponent, CurrentWeapon);
	DOREPLIFETIME_CONDITION(UTHWeaponComponent, bIsAiming, COND_SkipOwner);
}

void UTHWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// On every machine, before any weapon is equipped (weapons attach their third-person copy to this body)
	if (BodyAnimClass)
	{
		if (USkeletalMeshComponent* Body = GetBodyMesh())
		{
			Body->SetAnimInstanceClass(BodyAnimClass);
		}
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Pawn;
	SpawnParams.Instigator = Pawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const TSubclassOf<ATHWeaponBase>& WeaponClass : DefaultLoadout)
	{
		if (WeaponClass)
		{
			if (ATHWeaponBase* Weapon = GetWorld()->SpawnActor<ATHWeaponBase>(WeaponClass, Pawn->GetActorTransform(), SpawnParams))
			{
				Weapons.Add(Weapon);
			}
		}
	}

	if (!Weapons.IsEmpty())
	{
		SetCurrentWeapon(Weapons[0]);
	}
}

void UTHWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveMappingContext();

	if (GetOwner()->HasAuthority())
	{
		for (ATHWeaponBase* Weapon : Weapons)
		{
			if (Weapon)
			{
				Weapon->Destroy();
			}
		}
		Weapons.Empty();
	}

	Super::EndPlay(EndPlayReason);
}

// Input

void UTHWeaponComponent::SetupPlayerInput(UEnhancedInputComponent* InputComponent)
{
	if (!InputComponent)
	{
		return;
	}

	if (!WeaponMappingContext)
	{
		BuildDefaultInput();
	}
	AddMappingContext();

	if (FireAction)
	{
		InputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &UTHWeaponComponent::OnFirePressed);
		InputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &UTHWeaponComponent::OnFireReleased);
	}
	if (AimAction)
	{
		InputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &UTHWeaponComponent::OnAimPressed);
		InputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &UTHWeaponComponent::OnAimReleased);
	}
	if (ReloadAction)
	{
		InputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &UTHWeaponComponent::OnReloadPressed);
	}
	if (NextWeaponAction)
	{
		InputComponent->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &UTHWeaponComponent::OnNextWeapon);
	}
	if (PreviousWeaponAction)
	{
		InputComponent->BindAction(PreviousWeaponAction, ETriggerEvent::Started, this, &UTHWeaponComponent::OnPreviousWeapon);
	}
	if (CycleFireModeAction)
	{
		InputComponent->BindAction(CycleFireModeAction, ETriggerEvent::Started, this, &UTHWeaponComponent::OnCycleFireMode);
	}
	for (int32 Index = 0; Index < EquipSlotActions.Num(); ++Index)
	{
		if (EquipSlotActions[Index])
		{
			InputComponent->BindAction(EquipSlotActions[Index], ETriggerEvent::Started, this, &UTHWeaponComponent::OnEquipSlot, Index);
		}
	}
}

void UTHWeaponComponent::BuildDefaultInput()
{
	WeaponMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Weapons_Default"));

	auto MakeAction = [this](TObjectPtr<UInputAction>& Action, const TCHAR* Name, const FKey& Key)
	{
		if (!Action)
		{
			Action = NewObject<UInputAction>(this, Name);
		}
		WeaponMappingContext->MapKey(Action, Key);
	};

	MakeAction(FireAction, TEXT("IA_Fire"), EKeys::LeftMouseButton);
	MakeAction(AimAction, TEXT("IA_Aim"), EKeys::RightMouseButton);
	MakeAction(ReloadAction, TEXT("IA_Reload"), EKeys::R);
	MakeAction(NextWeaponAction, TEXT("IA_NextWeapon"), EKeys::MouseScrollDown);
	MakeAction(PreviousWeaponAction, TEXT("IA_PreviousWeapon"), EKeys::MouseScrollUp);
	MakeAction(CycleFireModeAction, TEXT("IA_CycleFireMode"), EKeys::B);

	const FKey SlotKeys[] = { EKeys::One, EKeys::Two, EKeys::Three };
	EquipSlotActions.SetNum(UE_ARRAY_COUNT(SlotKeys));
	for (int32 Index = 0; Index < EquipSlotActions.Num(); ++Index)
	{
		MakeAction(EquipSlotActions[Index], *FString::Printf(TEXT("IA_EquipSlot%d"), Index + 1), SlotKeys[Index]);
	}
}

void UTHWeaponComponent::AddMappingContext()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || !WeaponMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		Subsystem->AddMappingContext(WeaponMappingContext, MappingContextPriority);
		InputSubsystem = Subsystem;
	}
}

void UTHWeaponComponent::RemoveMappingContext()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = InputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(WeaponMappingContext);
	}
	InputSubsystem.Reset();
}

void UTHWeaponComponent::OnFirePressed()
{
	if (CurrentWeapon && !bWeaponsBlocked)
	{
		CurrentWeapon->StartFire();
	}
}

void UTHWeaponComponent::OnFireReleased()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
	}
}

void UTHWeaponComponent::OnAimPressed()
{
	SetAiming(true);
}

void UTHWeaponComponent::OnAimReleased()
{
	SetAiming(false);
}

void UTHWeaponComponent::OnReloadPressed()
{
	if (CurrentWeapon && !bWeaponsBlocked)
	{
		CurrentWeapon->StartReload();
	}
}

void UTHWeaponComponent::OnNextWeapon()
{
	CycleWeapon(1);
}

void UTHWeaponComponent::OnPreviousWeapon()
{
	CycleWeapon(-1);
}

void UTHWeaponComponent::OnCycleFireMode()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->CycleFireMode();
	}
}

void UTHWeaponComponent::OnEquipSlot(int32 Index)
{
	EquipWeaponAtIndex(Index);
}

// Switching

void UTHWeaponComponent::CycleWeapon(int32 Direction)
{
	if (Weapons.Num() < 2)
	{
		return;
	}

	const int32 CurrentIndex = FMath::Max(Weapons.IndexOfByKey(CurrentWeapon), 0);
	EquipWeaponAtIndex((CurrentIndex + Direction + Weapons.Num()) % Weapons.Num());
}

void UTHWeaponComponent::EquipWeaponAtIndex(int32 Index)
{
	if (bWeaponsBlocked || !Weapons.IsValidIndex(Index) || Weapons[Index] == CurrentWeapon)
	{
		return;
	}

	ATHWeaponBase* NewWeapon = Weapons[Index];

	// Switch locally right away for responsiveness; the server confirms through replication
	SetCurrentWeapon(NewWeapon);
	if (!GetOwner()->HasAuthority())
	{
		ServerEquipWeapon(NewWeapon);
	}
}

void UTHWeaponComponent::ServerEquipWeapon_Implementation(ATHWeaponBase* NewWeapon)
{
	if (!bWeaponsBlocked && Weapons.Contains(NewWeapon))
	{
		SetCurrentWeapon(NewWeapon);
	}
}

void UTHWeaponComponent::SetCurrentWeapon(ATHWeaponBase* NewWeapon)
{
	if (NewWeapon == CurrentWeapon)
	{
		return;
	}

	ATHWeaponBase* OldWeapon = CurrentWeapon;
	CurrentWeapon = NewWeapon;
	OnRep_CurrentWeapon(OldWeapon);
}

void UTHWeaponComponent::OnRep_CurrentWeapon(ATHWeaponBase* OldWeapon)
{
	if (OldWeapon && OldWeapon != CurrentWeapon)
	{
		OldWeapon->OnUnequipped();
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->OnEquipped();
		CurrentWeapon->SetFiringBlocked(bWeaponsBlocked);
	}

	OnWeaponChanged.Broadcast(CurrentWeapon, OldWeapon);
}

void UTHWeaponComponent::SetWeaponsBlocked(bool bBlocked)
{
	bWeaponsBlocked = bBlocked;

	if (CurrentWeapon)
	{
		CurrentWeapon->SetFiringBlocked(bBlocked);
	}
	if (bBlocked)
	{
		SetAiming(false);
	}
}

// Aiming

void UTHWeaponComponent::SetAiming(bool bNewAiming)
{
	if (bNewAiming && (bWeaponsBlocked || !CurrentWeapon))
	{
		return;
	}

	if (bIsAiming == bNewAiming)
	{
		return;
	}

	bIsAiming = bNewAiming;

	// Only the owning client initiates aiming, so only it needs to tell the server
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled() && !Pawn->HasAuthority())
	{
		ServerSetAiming(bNewAiming);
	}
}

void UTHWeaponComponent::ServerSetAiming_Implementation(bool bNewAiming)
{
	SetAiming(bNewAiming);
}

float UTHWeaponComponent::GetMovementSpeedMultiplier() const
{
	const UTHWeaponData* Data = CurrentWeapon ? CurrentWeapon->GetWeaponData() : nullptr;
	if (!Data)
	{
		return 1.0f;
	}

	return Data->MoveSpeedMultiplier * (bIsAiming ? Data->AimMoveSpeedMultiplier : 1.0f);
}

void UTHWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		UpdateAimFieldOfView(DeltaTime);
	}
}

UCameraComponent* UTHWeaponComponent::FindViewCamera() const
{
	TInlineComponentArray<UCameraComponent*> Cameras(GetOwner());
	UCameraComponent* Result = nullptr;
	for (UCameraComponent* Camera : Cameras)
	{
		if (!Result || Camera->IsActive())
		{
			Result = Camera;
		}
	}
	return Result;
}

void UTHWeaponComponent::UpdateAimFieldOfView(float DeltaTime)
{
	const UTHWeaponData* Data = CurrentWeapon ? CurrentWeapon->GetWeaponData() : nullptr;
	const float AimTime = Data ? Data->AimTime : 0.15f;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bIsAiming ? 1.0f : 0.0f, DeltaTime, 1.0f / AimTime);

	UCameraComponent* Camera = FindViewCamera();
	if (!Camera)
	{
		return;
	}

	// Leave the camera alone at rest so the character Blueprint's own zoom and night vision FOV still work
	if (AimAlpha <= 0.0f)
	{
		if (bAppliedAimFieldOfView)
		{
			Camera->SetFieldOfView(HipFieldOfView);
			bAppliedAimFieldOfView = false;
		}
		return;
	}

	if (!bAppliedAimFieldOfView)
	{
		HipFieldOfView = Camera->FieldOfView;
		bAppliedAimFieldOfView = true;
	}

	const float AimFOV = Data ? Data->AimFieldOfView : HipFieldOfView;
	Camera->SetFieldOfView(FMath::InterpEaseInOut(HipFieldOfView, AimFOV, AimAlpha, 2.0f));
}

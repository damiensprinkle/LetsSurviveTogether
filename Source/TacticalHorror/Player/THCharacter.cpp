#include "Player/THCharacter.h"
#include "EngineUtils.h"
#include "Enemies/THEnemyCharacter.h"
#include "Components/LightComponent.h"
#include "Multiplayer/THDoorSyncComponent.h"
#include "Multiplayer/THValveDoorComponent.h"
#include "Player/THCharacterMovementComponent.h"
#include "Player/THPlayerController.h"
#include "Weapons/THWeaponBase.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/Components/THFootstepComponent.h"
#include "Weapons/THWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

// Matches the "Weapon" trace channel in DefaultEngine.ini
static constexpr ECollisionChannel WeaponTraceChannel = ECC_GameTraceChannel1;

static TAutoConsoleVariable<bool> CVarDebugView(
	TEXT("th.DebugView"),
	false,
	TEXT("Show the local player's final camera view and active post-process volumes on screen."));

namespace
{
	void DrawViewDebug(const ATHCharacter* Character)
	{
		const APlayerController* PC = Cast<APlayerController>(Character->GetController());
		if (!GEngine || !PC || !PC->PlayerCameraManager)
		{
			return;
		}

		int32 Key = 7700;
		auto Line = [&Key](const FString& Text, const FColor& Color = FColor::Yellow)
		{
			GEngine->AddOnScreenDebugMessage(Key++, 0.0f, Color, Text);
		};

		const FMinimalViewInfo& POV = PC->PlayerCameraManager->GetCameraCacheView();
		const bool bBad = POV.Location.ContainsNaN() || POV.Rotation.ContainsNaN() || !FMath::IsFinite(POV.FOV) || POV.FOV < 5.0f || POV.FOV > 170.0f;
		Line(FString::Printf(TEXT("FINAL VIEW  target=%s  loc=%s  rot=%s  FOV=%.3f  ortho=%d  aspect=%.3f"),
			*GetNameSafe(PC->PlayerCameraManager->GetViewTarget()), *POV.Location.ToCompactString(), *POV.Rotation.ToCompactString(),
			POV.FOV, POV.ProjectionMode == ECameraProjectionMode::Orthographic, POV.AspectRatio), bBad ? FColor::Red : FColor::Green);
		Line(FString::Printf(TEXT("  pp blend weight=%.3f  pawn loc=%s  vel=%s"), POV.PostProcessBlendWeight,
			*Character->GetActorLocation().ToCompactString(), *Character->GetVelocity().ToCompactString()));

		TInlineComponentArray<UCameraComponent*> Cameras(Character);
		for (const UCameraComponent* Camera : Cameras)
		{
			Line(FString::Printf(TEXT("  camera %s active=%d FOV=%.3f world=%s  ppWeight=%.2f"), *Camera->GetName(), Camera->IsActive(),
				Camera->FieldOfView, *Camera->GetComponentLocation().ToCompactString(), Camera->PostProcessBlendWeight));
		}

		TInlineComponentArray<UPostProcessComponent*> PostProcess(Character);
		for (const UPostProcessComponent* PP : PostProcess)
		{
			Line(FString::Printf(TEXT("  postprocess %s enabled=%d weight=%.2f tint=%s"), *PP->GetName(), PP->bEnabled, PP->BlendWeight,
				*PP->Settings.SceneColorTint.ToString()));
		}

		// Everything visible near the camera: the pawn, actors attached to it (weapons), and anything with collision nearby
		constexpr float Radius = 50.0f;
		TSet<const UPrimitiveComponent*> Candidates;

		TArray<AActor*> Actors = { const_cast<ATHCharacter*>(Character) };
		Character->GetAttachedActors(Actors, false, true);
		for (const AActor* Actor : Actors)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
			Candidates.Append(Primitives);
		}

		TArray<FOverlapResult> Overlaps;
		Character->GetWorld()->OverlapMultiByObjectType(Overlaps, POV.Location, FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeSphere(Radius));
		for (const FOverlapResult& Overlap : Overlaps)
		{
			Candidates.Add(Overlap.GetComponent());
		}

		for (const UPrimitiveComponent* Prim : Candidates)
		{
			if (!Prim || !Prim->IsVisible() || Prim->bHiddenInGame || !Prim->IsRegistered())
			{
				continue;
			}

			const float Distance = FMath::Sqrt(Prim->Bounds.GetBox().ComputeSquaredDistanceToPoint(POV.Location));
			if (Distance > Radius)
			{
				continue;
			}

			const UMaterialInterface* Material = Prim->GetNumMaterials() > 0 ? Prim->GetMaterial(0) : nullptr;
			const bool bOwnerSees = !Prim->bOwnerNoSee;
			Line(FString::Printf(TEXT("  NEAR CAMERA %.0fcm: %s.%s [%s] mat=%s ownerSees=%d boundsRadius=%.0f"),
				Distance, *GetNameSafe(Prim->GetOwner()), *Prim->GetName(), *Prim->GetClass()->GetName(), *GetNameSafe(Material),
				bOwnerSees, Prim->Bounds.SphereRadius), bOwnerSees ? FColor::Orange : FColor::Silver);
		}
	}
}

ATHCharacter::ATHCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTHCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	TemplateInteractAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprint/Input/Actions/IA_Interact.IA_Interact")));

	// Sounds (developer's picks, 2026-11-11): MaleCharVocLITE hit reactions and jumping, HealthDamageSFX death stingers
	auto Sounds = [](const TCHAR* Folder, const TCHAR* Prefix, std::initializer_list<const TCHAR*> Numbers)
	{
		TArray<TSoftObjectPtr<USoundBase>> List;
		for (const TCHAR* Number : Numbers)
		{
			const FString Name = FString::Printf(TEXT("%s%s_Cue"), Prefix, Number);
			List.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Folder, *Name, *Name))));
		}
		return List;
	};
	HurtSounds = Sounds(TEXT("/Game/ThirdParty/MaleCharVocLITE/Cues/Combat/Hit_Reactions"), TEXT("VOXReac_HitReaction_HA_MaleCharVoc_"), { TEXT("01"), TEXT("04"), TEXT("31"), TEXT("36") });
	JumpSounds = Sounds(TEXT("/Game/ThirdParty/MaleCharVocLITE/Cues/Physical_States/Jumping"), TEXT("VOXMale_Jumping_HA_MaleCharVoc_"), { TEXT("02"), TEXT("07"), TEXT("48"), TEXT("57") });
	DeathSounds = Sounds(TEXT("/Game/ThirdParty/HealthDamageSFX/Cues/Death"), TEXT("DSGNStngr_DeathCue_HA_HealthDmg_"),
		{ TEXT("01"), TEXT("02"), TEXT("03"), TEXT("04"), TEXT("05"), TEXT("06"), TEXT("07"), TEXT("08"), TEXT("09"), TEXT("10"), TEXT("11"), TEXT("12"), TEXT("13"), TEXT("14"), TEXT("15"), TEXT("16") });
	VoiceAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(TEXT("/Game/Audio/SA_Voice.SA_Voice")));

	// Death performances (Darkania Works, CC BY 4.0): all but _04, which never falls
	for (const int32 Number : { 0, 1, 2, 3, 5, 6, 7, 8, 9, 10 })
	{
		const FString Name = FString::Printf(TEXT("SK_DramaticDeath_%02d"), Number);
		DeathPerformances.Add(TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("/Game/ThirdParty/DramaticDeath/Animations/%s.%s"), *Name, *Name))));
	}

	// Component names avoid clashing with anything the template Blueprint already defines
	HealthComponent = CreateDefaultSubobject<UTHHealthComponent>(TEXT("THHealth"));
	WeaponComponent = CreateDefaultSubobject<UTHWeaponComponent>(TEXT("THWeapons"));
	FootstepComponent = CreateDefaultSubobject<UTHFootstepComponent>(TEXT("THFootsteps"));

	// Bullets pass the capsule and hit the skeletal mesh, so hits know which bone (headshots)
	GetCapsuleComponent()->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Block);
	// Blood droplets pass through living bodies (they hung in the air on the invisible capsule)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
}

void ATHCharacter::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnHealthStateChanged.AddDynamic(this, &ATHCharacter::HandleHealthStateChanged);
	HealthComponent->OnHealthChanged.AddDynamic(this, &ATHCharacter::HandleHealthChanged);
	LastSeenHealth = HealthComponent->GetHealth();
}

void ATHCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// The capsule's centre drops by the adjustment while its base stays put; raise the body by the same amount.
	// GetMesh() is already handled by ACharacter.
	USkeletalMeshComponent* Body = WeaponComponent ? WeaponComponent->GetBodyMesh() : nullptr;
	if (Body && Body != GetMesh())
	{
		Body->AddRelativeLocation(FVector(0.0f, 0.0f, HalfHeightAdjust));
	}
}

void ATHCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	USkeletalMeshComponent* Body = WeaponComponent ? WeaponComponent->GetBodyMesh() : nullptr;
	if (Body && Body != GetMesh())
	{
		Body->AddRelativeLocation(FVector(0.0f, 0.0f, -HalfHeightAdjust));
	}
}

void ATHCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// The Blueprint's key events are bound just above. The template's L flashlight is replaced by the weapon light
	// (UTHWeaponComponent) and the hand flashlight (key 3), so drop its L binding.
	if (InputComponent)
	{
		InputComponent->KeyBindings.RemoveAll([](const FInputKeyBinding& Binding) { return Binding.Chord.Key == EKeys::L; });
	}
}

void ATHCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	WeaponComponent->SetupPlayerInput(EnhancedInput);

	if (!ReviveAction)
	{
		// E is shared with the template's interact, so revive lets it through
		ReviveAction = NewObject<UInputAction>(this, TEXT("IA_Revive"));
		ReviveAction->bConsumeInput = false;

		// Shift is consumed: at this priority it hides the template's IA_Sprint binding, so there is no sprint
		WalkQuietlyAction = NewObject<UInputAction>(this, TEXT("IA_WalkQuietly"));
		WalkQuietlyAction->bConsumeInput = true;

		ValveAction = NewObject<UInputAction>(this, TEXT("IA_Valve"));
		ValveAction->bConsumeInput = false;

		UInputMappingContext* Context = NewObject<UInputMappingContext>(this, TEXT("IMC_THCharacter_Default"));
		Context->MapKey(ReviveAction, EKeys::E);
		Context->MapKey(WalkQuietlyAction, EKeys::LeftShift);
		Context->MapKey(ValveAction, EKeys::F);

		const APlayerController* PC = Cast<APlayerController>(GetController());
		const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
		{
			Subsystem->AddMappingContext(Context, 2);
		}
	}

	EnhancedInput->BindAction(ReviveAction, ETriggerEvent::Started, this, &ATHCharacter::StartRevive);
	EnhancedInput->BindAction(ReviveAction, ETriggerEvent::Completed, this, &ATHCharacter::StopRevive);
	EnhancedInput->BindAction(WalkQuietlyAction, ETriggerEvent::Started, this, &ATHCharacter::StartWalkQuietly);
	EnhancedInput->BindAction(WalkQuietlyAction, ETriggerEvent::Completed, this, &ATHCharacter::StopWalkQuietly);
	EnhancedInput->BindAction(ValveAction, ETriggerEvent::Started, this, &ATHCharacter::HandleValvePressed);
	EnhancedInput->BindAction(ValveAction, ETriggerEvent::Completed, this, &ATHCharacter::HandleValveReleased);
	if (const UInputAction* InteractAction = TemplateInteractAction.LoadSynchronous())
	{
		// Alongside the Blueprint's own binding, which keeps handling everything else
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ATHCharacter::HandleInteractPressed);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &ATHCharacter::HandleInteractReleased);
	}
}

void ATHCharacter::HandleInteractPressed()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	// The same test as the template's TraceForInteraction, so this press and the Blueprint's agree on what is in front
	FVector Eye;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Eye, Rotation);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Eye, Eye + Rotation.Vector() * InteractRange, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(5.0f), FCollisionQueryParams(SCENE_QUERY_STAT(THInteract), false, this));
	const AActor* Target = bHit ? Hit.GetActor() : nullptr;
	UTHPressableComponent* Pressable = Target ? Target->FindComponentByClass<UTHPressableComponent>() : nullptr;
	if (Pressable && Pressable->UsesInteractKey())
	{
		// Held things (target buttons) are told when the key comes up
		HeldInteract = Pressable;
		ServerPress(Pressable);
	}
}

void ATHCharacter::HandleInteractReleased()
{
	if (HeldInteract)
	{
		ServerRelease(HeldInteract);
		HeldInteract = nullptr;
	}
}

void ATHCharacter::HandleValvePressed()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		UTHValveDoorComponent* Valve = It->FindComponentByClass<UTHValveDoorComponent>();
		if (Valve && Valve->IsPlayerAtValve(this))
		{
			HeldValve = Valve;
			ServerPress(Valve);
			return;
		}
	}
}

void ATHCharacter::HandleValveReleased()
{
	if (HeldValve)
	{
		ServerRelease(HeldValve);
		HeldValve = nullptr;
	}
}

void ATHCharacter::ServerPress_Implementation(UTHPressableComponent* Target)
{
	if (Target && Target->IsWithinReach(this))
	{
		Target->ServerPressed(this);
	}
}

void ATHCharacter::ServerRelease_Implementation(UTHPressableComponent* Target)
{
	if (Target)
	{
		Target->ServerReleased(this);
	}
}

void ATHCharacter::ServerRequestDoorUnlock_Implementation(UTHDoorSyncComponent* Door)
{
	if (Door)
	{
		Door->HandleUnlockRequest(this);
	}
}

void ATHCharacter::StartWalkQuietly()
{
	CastChecked<UTHCharacterMovementComponent>(GetCharacterMovement())->SetWalkQuietly(true);
}

void ATHCharacter::StopWalkQuietly()
{
	CastChecked<UTHCharacterMovementComponent>(GetCharacterMovement())->SetWalkQuietly(false);
}

void ATHCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A jump: leaving the ground upwards (movement replicates, so every machine sees it)
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->IsFalling() && bWasOnGround && GetVelocity().Z > 150.0f && !bRagdoll)
	{
		PlayRandomSound(JumpSounds, false);
	}
	bWasOnGround = Movement->IsMovingOnGround();

	if (IsLocallyControlled())
	{
		UpdateLightFlicker(DeltaSeconds);
	}

	if (CVarDebugView.GetValueOnGameThread() && IsLocallyControlled())
	{
		DrawViewDebug(this);
	}

	// Server cancels a revive if either player moves away or goes down
	if (HasAuthority() && ReviveTarget.IsValid() && !CanReviveTarget(ReviveTarget.Get()))
	{
		ServerStopRevive_Implementation();
	}
}

float ATHCharacter::GetMovementSpeedMultiplier() const
{
	float Multiplier = WeaponComponent ? WeaponComponent->GetMovementSpeedMultiplier() : 1.0f;
	if (HealthComponent && HealthComponent->GetHealthState() == ETHHealthState::Downed)
	{
		Multiplier *= DownedSpeedMultiplier;
	}
	return Multiplier;
}

void ATHCharacter::HandleHealthStateChanged(UTHHealthComponent* Health, ETHHealthState NewState, ETHHealthState OldState)
{
	WeaponComponent->SetWeaponsBlocked(NewState != ETHHealthState::Alive);

	if (NewState == ETHHealthState::Dead)
	{
		GetCharacterMovement()->DisableMovement();
		if (HasAuthority() && !DeathPerformances.IsEmpty())
		{
			DeathPerformance = static_cast<uint8>(FMath::RandHelper(DeathPerformances.Num()));
			ForceNetUpdate();
		}
		BecomeRagdoll();
		PlayRandomSound(DeathSounds, true, IsLocallyControlled() ? 1.0f : OtherPlayerDeathVolume);
	}
	else if (OldState == ETHHealthState::Dead)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	if (NewState != ETHHealthState::Alive)
	{
		StopRevive();
	}
	// Downed holds a crouch (UTHCharacterMovementComponent); revived, they get back up
	else if (OldState == ETHHealthState::Downed)
	{
		UnCrouch();
	}

	ReceiveHealthStateChanged(NewState, OldState);
}

void ATHCharacter::HandleHealthChanged(UTHHealthComponent* Health, float NewHealth, AActor* DamageCauser)
{
	const float Lost = LastSeenHealth - NewHealth;
	LastSeenHealth = NewHealth;
	if (Lost <= 0.0f)
	{
		return;
	}
	// Heard by everyone (not the killing blow: that has the death sound)
	if (NewHealth > 0.0f && GetWorld()->GetTimeSeconds() >= NextHurtSoundTime)
	{
		NextHurtSoundTime = GetWorld()->GetTimeSeconds() + HurtSoundCooldown;
		PlayRandomSound(HurtSounds, false);
	}
	if (!IsLocallyControlled())
	{
		return;
	}
	if (ATHPlayerController* PC = Cast<ATHPlayerController>(GetController()))
	{
		PC->NotifyLocalHit(Lost);
	}
}

void ATHCharacter::PlayRandomSound(const TArray<TSoftObjectPtr<USoundBase>>& Sounds, bool b2D, float Volume)
{
	if (Sounds.IsEmpty() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Sound = Sounds[FMath::RandHelper(Sounds.Num())].LoadSynchronous();
	if (!Sound)
	{
		return;
	}
	LastSound = Sound;
	if (b2D)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume);
		return;
	}
	UGameplayStatics::SpawnSoundAttached(Sound, GetRootComponent(), NAME_None, FVector(0.0f, 0.0f, 60.0f),
		EAttachLocation::KeepRelativeOffset, true, Volume, 1.0f, 0.0f, VoiceAttenuation.LoadSynchronous());
}

void ATHCharacter::BecomeRagdoll()
{
	if (bRagdoll)
	{
		return;
	}
	bRagdoll = true;

	// The owner's first-person weapon (with its arms) goes; the view falls (UTHCameraModifier)
	if (IsLocallyControlled())
	{
		if (ATHWeaponBase* Weapon = WeaponComponent->GetCurrentWeapon())
		{
			if (USkeletalMeshComponent* FirstPerson = Weapon->GetMesh())
			{
				FirstPerson->SetVisibility(false, true);
			}
		}
	}

	// A death performance if there are any: now if the pick is known here, else when it arrives (OnRep_DeathPerformance)
	if (!DeathPerformances.IsEmpty())
	{
		PlayDeathPerformance();
		return;
	}

	USkeletalMeshComponent* Body = WeaponComponent->GetBodyMesh();
	if (!Body || !Body->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s's body has no physics asset: it can't fall as a ragdoll"), *GetName());
		return;
	}
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	// Folds forward rather than standing stiffly
	Body->AddImpulse(GetActorForwardVector() * 120.0f, NAME_None, true);
}

void ATHCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATHCharacter, DeathPerformance);
}

void ATHCharacter::OnRep_DeathPerformance()
{
	if (HealthComponent && HealthComponent->GetHealthState() == ETHHealthState::Dead)
	{
		PlayDeathPerformance();
	}
}

void ATHCharacter::PlayDeathPerformance()
{
	if (bPlayingDeathPerformance || !DeathPerformances.IsValidIndex(DeathPerformance))
	{
		return;
	}
	USkeletalMeshComponent* Body = WeaponComponent->GetBodyMesh();
	UAnimSequence* Performance = DeathPerformances[DeathPerformance].LoadSynchronous();
	if (!Body || !Performance)
	{
		return;
	}
	bPlayingDeathPerformance = true;
	// Played once from the start, and held on its last frame (on the floor)
	Body->PlayAnimation(Performance, false);
}

UAnimSequence* ATHCharacter::GetDeathPerformance() const
{
	const USkeletalMeshComponent* Body = WeaponComponent ? WeaponComponent->GetBodyMesh() : nullptr;
	const UAnimSingleNodeInstance* Single = Body && bPlayingDeathPerformance ? Body->GetSingleNodeInstance() : nullptr;
	return Single ? Cast<UAnimSequence>(Single->GetAnimationAsset()) : nullptr;
}

// Revive

bool ATHCharacter::CanReviveTarget(const ATHCharacter* Target) const
{
	return Target && Target != this
		&& HealthComponent->IsAlive()
		&& Target->GetHealthComponent()->GetHealthState() == ETHHealthState::Downed
		&& FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(ReviveRange);
}

ATHCharacter* ATHCharacter::FindReviveTarget() const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	GetActorEyesViewPoint(ViewLocation, ViewRotation);

	// Downed players are low to the ground, so use a forgiving sphere sweep rather than a precise line
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THReviveTrace), false, this);
	FHitResult Hit;
	const FVector End = ViewLocation + ViewRotation.Vector() * ReviveRange;
	if (GetWorld()->SweepSingleByChannel(Hit, ViewLocation, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.0f), Params))
	{
		ATHCharacter* Target = Cast<ATHCharacter>(Hit.GetActor());
		if (CanReviveTarget(Target))
		{
			return Target;
		}
	}
	return nullptr;
}

void ATHCharacter::StartRevive()
{
	ATHCharacter* Target = FindReviveTarget();
	if (!Target)
	{
		return;
	}

	LocalReviveStartTime = GetWorld()->GetTimeSeconds();
	if (HasAuthority())
	{
		ServerStartRevive_Implementation(Target);
	}
	else
	{
		ServerStartRevive(Target);
	}
}

void ATHCharacter::StopRevive()
{
	if (LocalReviveStartTime < 0.0f && !ReviveTarget.IsValid())
	{
		return;
	}

	LocalReviveStartTime = -1.0f;
	if (HasAuthority())
	{
		ServerStopRevive_Implementation();
	}
	else if (IsLocallyControlled())
	{
		ServerStopRevive();
	}
}

float ATHCharacter::GetReviveProgress() const
{
	if (LocalReviveStartTime < 0.0f || ReviveDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - LocalReviveStartTime) / ReviveDuration, 0.0f, 1.0f);
}

void ATHCharacter::ServerStartRevive_Implementation(ATHCharacter* Target)
{
	if (!CanReviveTarget(Target))
	{
		return;
	}

	ReviveTarget = Target;
	GetWorldTimerManager().SetTimer(ReviveTimer, this, &ATHCharacter::CompleteRevive, FMath::Max(ReviveDuration, 0.01f), false);
}

void ATHCharacter::ServerStopRevive_Implementation()
{
	GetWorldTimerManager().ClearTimer(ReviveTimer);
	ReviveTarget.Reset();
}

void ATHCharacter::CompleteRevive()
{
	if (ATHCharacter* Target = ReviveTarget.Get(); CanReviveTarget(Target))
	{
		Target->GetHealthComponent()->Revive(GetController());
	}
	ReviveTarget.Reset();
}

void ATHCharacter::ServerRequestDoor_Implementation(UTHDoorSyncComponent* Door, bool bOpen)
{
	if (Door)
	{
		Door->HandleRequest(this, bOpen);
	}
}

void ATHCharacter::UpdateLightFlicker(float DeltaSeconds)
{
	// The nearest stalker that plays with lights (developer, 2026-11-13): the closer, the worse
	float Strength = 0.0f;
	for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const float Reach = It->GetLightFlickerDistance();
		if (Reach > 0.0f)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			Strength = FMath::Max(Strength, 1.0f - Distance / Reach);
		}
	}
	FlickerStrength = Strength;

	// Every light this player carries (their own, their guns', the hand flashlight)
	TArray<AActor*> Carried;
	GetAttachedActors(Carried, true, true);
	Carried.Add(this);
	if (Strength <= 0.0f)
	{
		// Back to their own brightness
		for (const TPair<TWeakObjectPtr<ULightComponent>, float>& Each : FlickerBase)
		{
			if (ULightComponent* Light = Each.Key.Get())
			{
				Light->SetIntensity(Each.Value);
			}
		}
		FlickerBase.Reset();
		FlickerLevel = 1.0f;
		return;
	}
	// Steady a while, then a dip or a cut for a moment, then steady again: more often the closer it is (developer,
	// 2026-11-14: too quick; it used to pick a new level every 0.04-0.18 s)
	const double Time = GetWorld()->GetTimeSeconds();
	if (Time >= NextFlicker)
	{
		const bool bWasSteady = FlickerLevel >= 1.0f;
		const float Roll = FMath::FRand();
		FlickerLevel = !bWasSteady || Roll > Strength * 0.9f + 0.1f ? 1.0f : Roll < Strength * 0.4f ? 0.0f : FMath::FRandRange(0.15f, 0.6f);
		NextFlicker = Time + (FlickerLevel >= 1.0f
			? FMath::FRandRange(FlickerSteadyTime.X, FlickerSteadyTime.Y) * FMath::Lerp(1.0f, 0.5f, Strength)
			: FMath::FRandRange(FlickerDipTime.X, FlickerDipTime.Y));
		// The crackle as it goes
		if (FlickerLevel < 1.0f && !FlickerSounds.IsEmpty() && Time >= NextFlickerSound && IsLocallyControlled())
		{
			NextFlickerSound = Time + FlickerSoundGap;
			if (USoundBase* Crackle = FlickerSounds[FMath::RandHelper(FlickerSounds.Num())])
			{
				UGameplayStatics::PlaySoundAtLocation(this, Crackle, GetPawnViewLocation(), FMath::Lerp(0.5f, 1.0f, Strength));
			}
		}
	}
	for (AActor* Holder : Carried)
	{
		TInlineComponentArray<ULightComponent*> Lights(Holder);
		for (ULightComponent* Light : Lights)
		{
			if (!FlickerBase.Contains(Light))
			{
				FlickerBase.Add(Light, Light->Intensity);
			}
			Light->SetIntensity(FlickerBase[Light] * FlickerLevel);
		}
	}
}

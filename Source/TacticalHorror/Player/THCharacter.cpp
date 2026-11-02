#include "Player/THCharacter.h"
#include "Player/THCharacterMovementComponent.h"
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

	// Component names avoid clashing with anything the template Blueprint already defines
	HealthComponent = CreateDefaultSubobject<UTHHealthComponent>(TEXT("THHealth"));
	WeaponComponent = CreateDefaultSubobject<UTHWeaponComponent>(TEXT("THWeapons"));
	FootstepComponent = CreateDefaultSubobject<UTHFootstepComponent>(TEXT("THFootsteps"));

	// Bullets pass the capsule and hit the skeletal mesh, so hits know which bone (headshots)
	GetCapsuleComponent()->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Block);
}

void ATHCharacter::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnHealthStateChanged.AddDynamic(this, &ATHCharacter::HandleHealthStateChanged);
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

		UInputMappingContext* Context = NewObject<UInputMappingContext>(this, TEXT("IMC_THCharacter_Default"));
		Context->MapKey(ReviveAction, EKeys::E);
		Context->MapKey(WalkQuietlyAction, EKeys::LeftShift);

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
	}
	else if (OldState == ETHHealthState::Dead)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	if (NewState != ETHHealthState::Alive)
	{
		StopRevive();
	}

	ReceiveHealthStateChanged(NewState, OldState);
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

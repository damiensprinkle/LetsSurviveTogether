#include "Enemies/THEnemyCharacter.h"

#include "Enemies/THEnemyAIController.h"
#include "Enemies/THEnemyAnimInstance.h"
#include "Enemies/THEnemyAnimSet.h"
#include "Enemies/THEnemyBehavior.h"
#include "Enemies/THEnemyAppearance.h"
#include "Enemies/THRetargetAnimInstance.h"
#include "GroomBindingAsset.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/THCameraModifier.h"
#include "Player/Components/THFootstepComponent.h"
#include "Player/THPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

/** The "Weapon" channel bullets trace on (DefaultEngine.ini), as ATHWeaponBase::TraceChannel. */
static constexpr ECollisionChannel EnemyWeaponTraceChannel = ECC_GameTraceChannel1;
/** "BloodDroplets" (DefaultEngine.ini): the blood effects' droplets collide on it; living bodies let them through. */
static constexpr ECollisionChannel EnemyBloodDropletChannel = ECC_GameTraceChannel2;

ATHEnemyCharacter::ATHEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<UTHHealthComponent>(TEXT("THHealth"));
	// Twice a player's (developer, 2026-11-10)
	HealthComponent->SetDefaults(200.0f, false, false);

	FootstepComponent = CreateDefaultSubobject<UTHFootstepComponent>(TEXT("THFootsteps"));
	FootstepComponent->bMakesNoise = false;
	FootstepComponent->MinStepSpeed = 250.0f;	// running only: a shambling walk stays quiet
	// Each step as a foot lands (developer, 2026-11-16: the charger's stomps didn't line up with its feet)
	FootstepComponent->FootBones = { TEXT("foot_l"), TEXT("foot_r") };

	FleshMaterial = TSoftObjectPtr<UPhysicalMaterial>(FSoftObjectPath(TEXT("/Game/Effects/PhysicalMaterials/PM_Flesh.PM_Flesh")));

	AIControllerClass = ATHEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);
	// Bullets pass the capsule and hit the body's bones (physics asset), so hits know which bone (headshots)
	GetCapsuleComponent()->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Ignore);
	// Blood droplets too: on the invisible capsule they hung in the air
	GetCapsuleComponent()->SetCollisionResponseToChannel(EnemyBloodDropletChannel, ECR_Ignore);

	// UE4-mannequin-style bodies: feet at the origin, facing +Y
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Block);
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UTHEnemyAnimInstance::StaticClass());

	// Turns to face where it walks; attacking and watching turn it from code (TurnTowards)
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	Movement->RotationRate = FRotator(0.0f, 300.0f, 0.0f);
	Movement->MaxWalkSpeed = 150.0f;
	// A stalker crouches behind low cover (the body set's Crouch animation; bIsCrouched replicates)
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(55.0f);
}

void ATHEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATHEnemyCharacter, Death);
	DOREPLIFETIME(ATHEnemyCharacter, Attack);
	DOREPLIFETIME(ATHEnemyCharacter, HeadDown);
	DOREPLIFETIME(ATHEnemyCharacter, bCalmWalk);
	DOREPLIFETIME(ATHEnemyCharacter, bBeckoning);
	DOREPLIFETIME(ATHEnemyCharacter, LightFlickerDistance);
	DOREPLIFETIME(ATHEnemyCharacter, SeveredLimbs);
	DOREPLIFETIME(ATHEnemyCharacter, SeverDirection);
	DOREPLIFETIME(ATHEnemyCharacter, bCrawling);
	DOREPLIFETIME(ATHEnemyCharacter, bAggressive);
	DOREPLIFETIME_CONDITION(ATHEnemyCharacter, VoicePitch, COND_InitialOnly);
	DOREPLIFETIME(ATHEnemyCharacter, AppearanceSeed);
	DOREPLIFETIME(ATHEnemyCharacter, AppearanceInUse);
	DOREPLIFETIME(ATHEnemyCharacter, Action);
	DOREPLIFETIME(ATHEnemyCharacter, AnimationsInUse);
}

void ATHEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Shows the body in the editor too
	ApplyBody();
}

void ATHEnemyCharacter::ApplyBody()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (BodyMesh && Body->GetSkeletalMeshAsset() != BodyMesh)
	{
		Body->SetSkeletalMeshAsset(BodyMesh);
	}
	// Also clears an override left on the component (a placed body changed to a mesh with its own physics asset)
	if (Body->PhysicsAssetOverride != PhysicsAssetOverride)
	{
		Body->SetPhysicsAsset(PhysicsAssetOverride, true);
	}
}

void ATHEnemyCharacter::OnRep_AppearanceSeed()
{
	ApplyAppearance();
}

void ATHEnemyCharacter::ApplyAppearance()
{
	const UTHEnemyAppearance* Looks = AppearanceInUse;
	if (!Looks || Looks->Looks.IsEmpty() || AppearanceSeed == 0 || AppliedAppearanceSeed == AppearanceSeed)
	{
		return;
	}
	AppliedAppearanceSeed = AppearanceSeed;
	FRandomStream Random(AppearanceSeed);
	// Its own footsteps if these bodies have some (a charger's stomp), on every machine
	FootstepComponent->SetStepSoundOverride(Looks->FootstepSounds, Looks->FootstepVolume);

	// The look, by weight
	float Total = 0.0f;
	for (const FTHEnemyLook& Each : Looks->Looks)
	{
		Total += FMath::Max(Each.Weight, 0.0f);
	}
	float Roll = Random.FRandRange(0.0f, FMath::Max(Total, UE_KINDA_SMALL_NUMBER));
	const FTHEnemyLook* Look = &Looks->Looks.Last();
	for (const FTHEnemyLook& Each : Looks->Looks)
	{
		Roll -= FMath::Max(Each.Weight, 0.0f);
		if (Roll <= 0.0f)
		{
			Look = &Each;
			break;
		}
	}
	auto Pick = [&Random](const auto& Array) { return Array.IsEmpty() ? nullptr : Array[Random.RandHelper(Array.Num())].Get(); };
	auto Dress = [&Random, &Pick](USkeletalMeshComponent* Component, const TArray<FTHMaterialChoice>& Choices, const TArray<UMaterialInterface*>* Picked)
	{
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			UMaterialInterface* Material = Picked ? (*Picked)[Index] : Pick(Choices[Index].Materials);
			const int32 Slot = Component->GetMaterialIndex(Choices[Index].Slot);
			if (Material && Slot != INDEX_NONE)
			{
				Component->SetMaterial(Slot, Material);
			}
		}
	};

	// The body and its skin, heads and gore. A retargeted look (a MetaHuman): the animations play on its Animated Body,
	// hidden (it still poses, takes the shots and falls as the ragdoll), and the body you see copies its pose
	if (VisibleBody)
	{
		VisibleBody->DestroyComponent();
		VisibleBody = nullptr;
	}
	GetMesh()->SetHiddenInGame(false);
	GetMesh()->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Block);
	// Which body (kept, with the look, for the pieces that come off: they match the body)
	CurrentLook = Look;
	CurrentBodyIndex = Look->Bodies.IsEmpty() ? 0 : Random.RandHelper(Look->Bodies.Num());
	USkeletalMesh* Body = Look->Bodies.IsEmpty() ? nullptr : Look->Bodies[CurrentBodyIndex].Get();
	if (Look->IsRetargeted())
	{
		BodyMesh = Look->AnimatedBody;
		PhysicsAssetOverride = Look->Physics;
		ApplyBody();
		if (Body)
		{
			VisibleBody = NewObject<USkeletalMeshComponent>(this);
			VisibleBody->SetSkeletalMeshAsset(Body);
			VisibleBody->SetupAttachment(GetMesh());
			VisibleBody->SetGenerateOverlapEvents(false);
			VisibleBody->SetAnimInstanceClass(UTHRetargetAnimInstance::StaticClass());
			// The body you see takes the shots, by its own physics asset (developer, 2026-11-13: shots hit the hidden
			// zombie's male proportions under a woman's look); without one, the hidden body still does
			const bool bTakesShots = VisibleBody->GetPhysicsAsset() != nullptr;
			if (bTakesShots)
			{
				VisibleBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				VisibleBody->SetCollisionObjectType(ECC_Pawn);
				VisibleBody->SetCollisionResponseToAllChannels(ECR_Ignore);
				VisibleBody->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Block);
				if (UPhysicalMaterial* Flesh = FleshMaterial.LoadSynchronous())
				{
					VisibleBody->SetPhysMaterialOverride(Flesh);
				}
			}
			else
			{
				VisibleBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			// Nothing is ever drawn (the headless tests), or the server traces shots against it where nobody looks: pose it anyway
			if (!FApp::CanEverRender() || (bTakesShots && HasAuthority()))
			{
				VisibleBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			}
			VisibleBody->RegisterComponent();
			if (UTHRetargetAnimInstance* Copy = Cast<UTHRetargetAnimInstance>(VisibleBody->GetAnimInstance()))
			{
				Copy->SetSource(Look->Retargeter, GetMesh());
			}
			// Copies the pose after the animated mesh has its own for the frame
			VisibleBody->AddTickPrerequisiteComponent(GetMesh());
			GetMesh()->SetHiddenInGame(true);
			if (bTakesShots)
			{
				GetMesh()->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Ignore);
			}
			GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}
	else if (Body)
	{
		BodyMesh = Body;
		PhysicsAssetOverride = Look->Physics;
		ApplyBody();
	}
	USkeletalMeshComponent* Leader = GetVisibleBody();
	Dress(Leader, Look->Materials, nullptr);
	// Its feet, for its steps: the animated body's (a retargeted look's hidden one always poses)
	FootstepComponent->SetFootMesh(GetMesh());

	// Clothing and hair: new meshes following the body's animation (and its ragdoll); they don't take shots
	for (USkeletalMeshComponent* Old : Outfit)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Outfit.Reset();
	for (UGroomComponent* Old : Grooms)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Grooms.Reset();
	for (const FTHOutfitPart& Part : Look->Parts)
	{
		if (Part.Choices.IsEmpty())
		{
			continue;
		}
		const FTHOutfitChoice& Choice = Part.Choices[Random.RandHelper(Part.Choices.Num())];
		// One pick per material slot, shared by every piece of the choice so they match
		TArray<UMaterialInterface*> Picked;
		for (const FTHMaterialChoice& Slot : Choice.Materials)
		{
			Picked.Add(Pick(Slot.Materials));
		}
		for (USkeletalMesh* PieceMesh : Choice.Meshes)
		{
			if (!PieceMesh)
			{
				continue;
			}
			USkeletalMeshComponent* Piece = NewObject<USkeletalMeshComponent>(this);
			Piece->SetSkeletalMeshAsset(PieceMesh);
			Piece->SetupAttachment(Leader);
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetGenerateOverlapEvents(false);
			Piece->RegisterComponent();
			Piece->SetLeaderPoseComponent(Leader);
			Dress(Piece, Choice.Materials, &Picked);
			Outfit.Add(Piece);
		}
	}

	// Hair and eyebrows, on the piece their binding was made for (a MetaHuman's face), else the body
	for (const FTHGroom& Each : Look->Grooms)
	{
		if (!Each.Groom)
		{
			continue;
		}
		USkeletalMeshComponent* Wearer = Leader;
		const USkeletalMesh* BoundTo = Each.Binding ? Each.Binding->GetTargetSkeletalMesh() : nullptr;
		for (USkeletalMeshComponent* Piece : Outfit)
		{
			if (BoundTo && Piece && Piece->GetSkeletalMeshAsset() == BoundTo)
			{
				Wearer = Piece;
				break;
			}
		}
		UGroomComponent* Groom = NewObject<UGroomComponent>(this);
		Groom->SetupAttachment(Wearer);
		Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Groom->SetUseCards(Each.bUseCards);
		Groom->SetGroomAsset(Each.Groom, Wearer->GetSkeletalMeshAsset() == BoundTo ? Each.Binding.Get() : nullptr);
		// Up close the groom's nearest LODs are strands, which optimized MetaHumans strip out, so it is held on the
		// nearest LOD that really has cards. Not the first LOD merely typed as cards: the MetaHumans' grooms say cards for
		// LODs 2-4 but carry card geometry only for LOD 3, and held on LOD 2 the hair drew nothing at all (developer,
		// 2026-11-13: the siren was still bald)
		int32 CardsLOD = 0;
		if (Each.bUseCards && Each.Groom)
		{
			auto Nearest = [](int32 Best, int32 GroupIndex, int32 LODIndex, const UStaticMesh* Geometry)
			{
				return GroupIndex == 0 && LODIndex >= 0 && Geometry && (Best == INDEX_NONE || LODIndex < Best) ? LODIndex : Best;
			};
			int32 Found = INDEX_NONE;
			for (const FHairGroupsCardsSourceDescription& Cards : Each.Groom->GetHairGroupsCards())
			{
				Found = Nearest(Found, Cards.GroupIndex, Cards.LODIndex, Cards.GetMesh());
			}
			// A groom with no cards: its nearest mesh LOD (a hair "helmet")
			if (Found == INDEX_NONE)
			{
				for (const FHairGroupsMeshesSourceDescription& Meshes : Each.Groom->GetHairGroupsMeshes())
				{
					Found = Nearest(Found, Meshes.GroupIndex, Meshes.LODIndex, Meshes.ImportedMesh);
				}
			}
			CardsLOD = FMath::Max(Found, 0);
		}
		Groom->RegisterComponent();
		if (CardsLOD > 0)
		{
			Groom->SetForcedLOD(CardsLOD);
		}
		Grooms.Add(Groom);
	}

	// Limbs already off (a late joiner, or a look built after the news): shown again on the new meshes
	AppliedSeveredLimbs = 0;
	CollapsedBones.Reset();
	OnRep_SeveredLimbs();
}

USkeletalMeshComponent* ATHEnemyCharacter::GetVisibleBody() const
{
	return VisibleBody ? VisibleBody.Get() : GetMesh();
}

void ATHEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyBody();
	if (HasAuthority())
	{
		LightFlickerDistance = Behavior ? Behavior->LightFlickerDistance : 0.0f;
	}
	// A random look: the server picks, every machine builds the same one
	// Its own voice: a little higher or lower than the others
	if (HasAuthority())
	{
		VoicePitch = FMath::FRandRange(0.88f, 1.12f);
	}
	if (HasAuthority() && !AppearanceInUse)
	{
		AppearanceInUse = Behavior && Behavior->Appearance ? Behavior->Appearance.Get() : Appearance.Get();
	}
	if (HasAuthority() && Behavior && Behavior->Animations)
	{
		AnimationsInUse = Behavior->Animations;
	}
	if (HasAuthority() && AppearanceInUse && AppearanceSeed == 0)
	{
		AppearanceSeed = FMath::RandRange(1, MAX_int32 - 1);
	}
	ApplyAppearance();
	USkeletalMeshComponent* Body = GetMesh();
	if (UPhysicalMaterial* Flesh = FleshMaterial.LoadSynchronous())
	{
		Body->SetPhysMaterialOverride(Flesh);
	}
	if (HasAuthority())
	{
		// The server traces shots against bones even where nobody looks at this body
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	HealthComponent->OnHealthStateChanged.AddDynamic(this, &ATHEnemyCharacter::HandleHealthStateChanged);

	// Joined after it died: drop the body quietly
	ApplyDeath(false);
}

void ATHEnemyCharacter::SetBehavior(UTHEnemyBehavior* NewBehavior)
{
	if (!HasAuthority())
	{
		return;
	}
	Behavior = NewBehavior;
	LightFlickerDistance = Behavior ? Behavior->LightFlickerDistance : 0.0f;
	// A behaviour with animations of its own (a siren: a person's)
	AnimationsInUse = Behavior && Behavior->Animations ? Behavior->Animations.Get() : nullptr;
	// A behaviour with looks of its own (a charger: the mutants) changes the body, already playing or not
	if (Behavior && Behavior->Appearance && Behavior->Appearance != AppearanceInUse && HasActorBegunPlay())
	{
		AppearanceInUse = Behavior->Appearance;
		AppearanceSeed = FMath::RandRange(1, MAX_int32 - 1);
		ApplyAppearance();
		ForceNetUpdate();
	}
	if (ATHEnemyAIController* AI = Cast<ATHEnemyAIController>(GetController()))
	{
		AI->ApplyBehavior();
	}
}

float ATHEnemyCharacter::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	ETHLimb Limb = ETHLimb::None;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		LastHit.Direction = Point.ShotDirection.GetSafeNormal();
		LastHit.Bone = Point.HitInfo.BoneName;
		Limb = LimbOfBone(LastHit.Bone);

		// Where it came from, seen from the enemy (for its stagger): the side facing the shooter
		const FVector From = -LastHit.Direction.GetSafeNormal2D();
		const float Ahead = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), From);
		const float Right = FVector::DotProduct(GetActorRightVector().GetSafeNormal2D(), From);
		LastHitSide = From.IsNearlyZero() || Ahead > 0.5f ? ETHHitSide::Front : Ahead < -0.5f ? ETHHitSide::Back
			: Right > 0.0f ? ETHHitSide::Right : ETHHitSide::Left;
	}
	else
	{
		LastHit = FTHEnemyDeath();
		LastHitSide = ETHHitSide::Front;
	}
	const UTHHealthComponent* Health = GetHealthComponent();
	bLastHitHeavy = Limb == ETHLimb::Head || (Health && Damage >= Health->GetMaxHealth() * 0.25f);
	// Who did it, for its mind as it dies (by then it has let go of its controller)
	LastAttacker = EventInstigator ? EventInstigator->GetPawn() : nullptr;

	// The health component takes it from here (OnTakeAnyDamage)
	const float Taken = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

	// Wounds (developer, 2026-11-13: hits that matter): damage to a limb adds up until it comes off; the head only with a
	// strong killing headshot
	if (HasAuthority() && Behavior && Limb != ETHLimb::None && Damage > 0.0f)
	{
		LimbDamage[static_cast<uint8>(Limb)] += Damage;
		const bool bKilled = IsDead() || (Health && !Health->IsAlive());
		const bool bOff = Limb == ETHLimb::Head
			? Behavior->HeadOffDamage > 0.0f && bKilled && Damage >= Behavior->HeadOffDamage
			: Behavior->LimbHealth > 0.0f && LimbDamage[static_cast<uint8>(Limb)] >= Behavior->LimbHealth;
		if (bOff)
		{
			SeverLimb(Limb, LastHit.Direction);
		}
	}

	// Its mind learns who did it (a dead one has none)
	if (ATHEnemyAIController* AI = Cast<ATHEnemyAIController>(GetController()))
	{
		AI->NotifyDamaged(EventInstigator ? EventInstigator->GetPawn() : nullptr);
	}
	return Taken;
}

void ATHEnemyCharacter::HandleHealthStateChanged(UTHHealthComponent* Health, ETHHealthState NewState, ETHHealthState OldState)
{
	if (!HasAuthority() || NewState != ETHHealthState::Dead || Death.bDead)
	{
		return;
	}

	Death = LastHit;
	Death.bDead = true;
	OnRep_Death();
	ForceNetUpdate();

	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	AttackEndTime = -1.0e6;
	// Its mind hears of it last (an ambusher's pack wakes), then has nothing left to do
	if (ATHEnemyAIController* AI = Cast<ATHEnemyAIController>(GetController()))
	{
		AI->NotifyKilled(LastAttacker.Get());
	}
	DetachFromControllerPendingDestroy();
	if (CorpseLifeSpan > 0.0f)
	{
		GetWorldTimerManager().SetTimer(CorpseTimer, this, &ATHEnemyCharacter::RemoveCorpse, CorpseLifeSpan);
	}
}

void ATHEnemyCharacter::RemoveCorpse()
{
	// Never in front of anyone: with a player near, it waits and looks again (developer, 2026-11-13: bodies piled up)
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < CorpseKeepDistance)
		{
			GetWorldTimerManager().SetTimer(CorpseTimer, this, &ATHEnemyCharacter::RemoveCorpse, 10.0f);
			return;
		}
	}
	Destroy();
}

// Attacks

bool ATHEnemyCharacter::StartAttack(AActor* Target)
{
	if (!HasAuthority() || IsDead() || !Target || !GetAnimations() || GetAnimations()->Attacks.IsEmpty() || IsAttacking())
	{
		return false;
	}
	// Both arms off: a bite, if its set has one (its index after the arm attacks)
	const UTHEnemyAnimSet* Set = GetAnimations();
	const bool bArmless = IsLimbSevered(ETHLimb::LeftArm) && IsLimbSevered(ETHLimb::RightArm) && !Set->ArmlessAttacks.IsEmpty();
	const int32 Index = bArmless ? Set->Attacks.Num() + FMath::RandHelper(Set->ArmlessAttacks.Num()) : FMath::RandHelper(Set->Attacks.Num());
	const FTHEnemyAttack& Chosen = *Set->GetAttack(Index);
	if (!Chosen.Animation || Chosen.GetDuration() <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	++Attack.Count;
	Attack.Index = static_cast<uint8>(Index);
	AttackTarget = Target;
	AttackEndTime = GetWorld()->GetTimeSeconds() + Chosen.GetDuration();
	GetCharacterMovement()->StopMovementImmediately();
	OnRep_Attack();
	ForceNetUpdate();
	GetWorldTimerManager().SetTimer(AttackHitTimer, this, &ATHEnemyCharacter::LandAttack,
		FMath::Max(Chosen.GetHitDelay(), 0.01f), false);
	return true;
}

float ATHEnemyCharacter::BangOnDoor(const FVector& Point, const FVector& Normal)
{
	if (!HasAuthority() || IsDead() || !Behavior || IsAttacking())
	{
		return 0.0f;
	}
	// One of its attack swings (no one to hit: AttackTarget stays empty), the bang as the blow lands
	float Length = 0.8f;
	float HitDelay = 0.3f;
	const UTHEnemyAnimSet* Set = GetAnimations();
	if (Set && !Set->Attacks.IsEmpty())
	{
		const int32 Index = FMath::RandHelper(Set->Attacks.Num());
		const FTHEnemyAttack& Chosen = Set->Attacks[Index];
		if (Chosen.Animation && Chosen.GetDuration() > UE_KINDA_SMALL_NUMBER)
		{
			++Attack.Count;
			Attack.Index = static_cast<uint8>(Index);
			AttackTarget = nullptr;
			Length = Chosen.GetDuration();
			HitDelay = FMath::Max(Chosen.GetHitDelay(), 0.01f);
			AttackEndTime = GetWorld()->GetTimeSeconds() + Length;
			GetCharacterMovement()->StopMovementImmediately();
			OnRep_Attack();
			ForceNetUpdate();
		}
	}
	const TArray<TObjectPtr<USoundBase>>& Sounds = Behavior->DoorBangSounds;
	USoundBase* Sound = Sounds.IsEmpty() ? nullptr : Sounds[FMath::RandHelper(Sounds.Num())].Get();
	const float ShakeRadius = Behavior->DoorBangShakeRadius;
	GetWorldTimerManager().SetTimer(DoorBangTimer, FTimerDelegate::CreateWeakLambda(this, [this, Point, Normal, Sound, ShakeRadius]()
	{
		if (!IsDead())
		{
			++DoorBangCount;
			MulticastWallCrash(Point, Normal, nullptr, Sound, ShakeRadius, 0.25f);
		}
	}), HitDelay, false);
	return Length;
}

void ATHEnemyCharacter::PlayRoar()
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}
	++Action.Count;
	Action.Kind = 1;
	OnRep_Action();
	ForceNetUpdate();
}

float ATHEnemyCharacter::PlayStumble(float DownTime)
{
	if (!HasAuthority() || IsDead() || !GetAnimations())
	{
		return 0.0f;
	}
	++Action.Count;
	Action.Kind = 2;
	Action.DownTime = DownTime;
	OnRep_Action();
	ForceNetUpdate();
	return GetAnimations()->GetStumbleDuration(DownTime);
}

void ATHEnemyCharacter::OnRep_Action()
{
	UTHEnemyAnimInstance* Anim = Cast<UTHEnemyAnimInstance>(GetMesh()->GetAnimInstance());
	if (Action.Count == 0 || IsDead() || !Anim || !GetAnimations())
	{
		return;
	}
	if (Action.Kind == 1)
	{
		Anim->PlayAttack(GetAnimations()->Roar);
	}
	else if (Action.Kind == 2)
	{
		Anim->PlayStumble(Action.DownTime);
	}
	else if (Action.Kind == 3)
	{
		Anim->PlayLieDown(GetLyingPose(), Action.DownTime > 0.0f);
	}
	else if (Action.Kind == 6)
	{
		Anim->StopLying();
	}
	else if (Action.Kind == 7)
	{
		Anim->PlayLieDown(GetLyingPose());
		Anim->PlayGetUp();
	}
	else if (Action.Kind == 4)
	{
		Anim->PlayGetUp();
	}
	else if (Action.Kind == 5 && GetAnimations()->HitReactions.IsValidIndex(Action.Pose))
	{
		Anim->PlayHitReaction(GetAnimations()->HitReactions[Action.Pose].Clip, Action.DownTime);
	}
}

void ATHEnemyCharacter::LieDown(int32 ForcedPose, bool bBlend, bool bPlayingDead)
{
	const UTHEnemyAnimSet* Set = GetAnimations();
	if (!HasAuthority() || IsDead() || !Set || (!Set->StumbleDown && Set->LyingPoses.IsEmpty()))
	{
		return;
	}
	bLyingDown = true;
	// One of its poses at random (developer, 2026-11-13): waiting, those not kept for playing dead; playing dead, those kept
	// for it if there are any
	TArray<int32, TInlineAllocator<8>> Picks;
	for (int32 Index = 0; Index < FMath::Min(Set->LyingPoses.Num(), 255); ++Index)
	{
		if (Set->LyingPoses[Index].bPlayingDeadOnly == bPlayingDead)
		{
			Picks.Add(Index);
		}
	}
	if (Picks.IsEmpty())
	{
		for (int32 Index = 0; Index < FMath::Min(Set->LyingPoses.Num(), 255); ++Index)
		{
			Picks.Add(Index);
		}
	}
	Action.Pose = Set->LyingPoses.IsEmpty() ? 0 : Set->LyingPoses.IsValidIndex(ForcedPose) ? static_cast<uint8>(ForcedPose)
		: static_cast<uint8>(Picks[FMath::RandHelper(Picks.Num())]);
	Action.DownTime = bBlend ? 1.0f : 0.0f;
	++Action.Count;
	Action.Kind = 3;
	OnRep_Action();
	ForceNetUpdate();
}

void ATHEnemyCharacter::PlayHitReaction(ETHHitSide Side, bool bHeavy, float StoppedFor)
{
	const UTHEnemyAnimSet* Set = GetAnimations();
	const int32 Index = Set ? Set->PickHitReaction(Side, bHeavy) : INDEX_NONE;
	if (!HasAuthority() || IsDead() || Index == INDEX_NONE || Index > 255)
	{
		return;
	}
	Action.Pose = static_cast<uint8>(Index);
	Action.DownTime = StoppedFor;
	++Action.Count;
	Action.Kind = 5;
	OnRep_Action();
	ForceNetUpdate();
}

// Wounds

namespace
{
	/** Each limb's top bone (hidden, with everything below it) and the YI skin's wound mask for it. */
	struct FLimbInfo { const TCHAR* Bone; const TCHAR* GoreMask; };
	const FLimbInfo LimbInfo[] = {
		{ TEXT("head"), TEXT("1.HeadGoreMask") },
		{ TEXT("upperarm_l"), TEXT("5.LeftArmGoreMask") },
		{ TEXT("upperarm_r"), TEXT("4.RightArmGoreMask") },
		{ TEXT("thigh_l"), TEXT("3.LeftLegGoreMask") },
		{ TEXT("thigh_r"), TEXT("2.RightLegGoreMask") },
	};

	/** The material a body's slot was given, without any wound shown on it (a dynamic instance's parent). */
	UMaterialInterface* BaseMaterial(const USkeletalMeshComponent* Component, int32 Slot)
	{
		UMaterialInterface* Material = Component->GetMaterial(Slot);
		const UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
		return Dynamic && Dynamic->Parent ? Dynamic->Parent.Get() : Material;
	}
}

void ATHEnemyCharacter::MatchPieceMaterials(USkeletalMeshComponent* Piece, const USkeletalMeshComponent* Body,
	TConstArrayView<TObjectPtr<USkeletalMeshComponent>> Clothes)
{
	if (!Piece)
	{
		return;
	}
	// Each slot of the piece takes what the body (or a piece of clothing) has in the slot of the same name; the pack also
	// names some "<Model>_Body" for the body's "Body" (F02's limbs). Slots with no match keep the piece's own (the
	// Priest's pieces have different slots and their own materials)
	auto Matches = [](const FString& PieceSlot, const FString& BodySlot)
	{
		return PieceSlot.Equals(BodySlot, ESearchCase::IgnoreCase) || PieceSlot.EndsWith(TEXT("_") + BodySlot, ESearchCase::IgnoreCase);
	};
	const TArray<FName> PieceSlots = Piece->GetMaterialSlotNames();
	for (const FName& PieceSlot : PieceSlots)
	{
		const FString PieceName = PieceSlot.ToString();
		UMaterialInterface* Found = nullptr;
		auto Search = [&](const USkeletalMeshComponent* Source)
		{
			if (!Source || Found)
			{
				return;
			}
			for (const FName& SourceSlot : Source->GetMaterialSlotNames())
			{
				if (Matches(PieceName, SourceSlot.ToString()))
				{
					Found = BaseMaterial(Source, Source->GetMaterialIndex(SourceSlot));
					return;
				}
			}
		};
		Search(Body);
		for (const USkeletalMeshComponent* Each : Clothes)
		{
			Search(Each);
		}
		if (Found)
		{
			Piece->SetMaterial(Piece->GetMaterialIndex(PieceSlot), Found);
		}
	}
}

namespace
{
	/** Shows the wound on every skin of Component that has Mask (a dynamic instance of it, the mask full on). */
	void ShowWound(USkeletalMeshComponent* Component, FName Mask)
	{
		for (int32 Slot = 0; Component && Slot < Component->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Material = Component->GetMaterial(Slot);
			float Value = 0.0f;
			if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(Mask), Value))
			{
				continue;
			}
			UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
			if (!Dynamic)
			{
				Dynamic = Component->CreateDynamicMaterialInstance(Slot, Material);
			}
			if (Dynamic)
			{
				Dynamic->SetScalarParameterValue(Mask, 1.0f);
			}
		}
	}
}

ETHLimb ATHEnemyCharacter::LimbOfBone(FName Bone) const
{
	// The body the shot hit: a retargeted look's visible body takes the shots (its own skeleton)
	const USkeletalMeshComponent* Body = VisibleBody && VisibleBody->GetBoneIndex(Bone) != INDEX_NONE ? VisibleBody.Get() : GetMesh();
	if (!Body || Bone.IsNone())
	{
		return ETHLimb::None;
	}
	// Up from the bone hit to the first limb's top bone. The head is the guns' head bones only, not the neck: a neck hit
	// got the head's wounds and heavy flinch without the headshot damage (2026-11-13)
	for (FName Each = Bone; !Each.IsNone(); Each = Body->GetParentBone(Each))
	{
		if (HeadBoneNames.Contains(Each))
		{
			return ETHLimb::Head;
		}
		for (int32 Index = 1; Index < UE_ARRAY_COUNT(LimbInfo); ++Index)
		{
			if (Each == LimbInfo[Index].Bone)
			{
				return static_cast<ETHLimb>(Index);
			}
		}
	}
	return ETHLimb::None;
}

bool ATHEnemyCharacter::CanSever(ETHLimb Limb) const
{
	const FTHSeverableLimb* Entry = CurrentLook && Limb != ETHLimb::None ? CurrentLook->FindLimb(Limb) : nullptr;
	return Entry && !Entry->Meshes.IsEmpty();
}

bool ATHEnemyCharacter::SeverLimb(ETHLimb Limb, const FVector& Direction)
{
	if (!HasAuthority() || !CanSever(Limb) || IsLimbSevered(Limb))
	{
		return false;
	}
	SeverDirection = Direction.GetSafeNormal();
	SeveredLimbs |= 1 << static_cast<uint8>(Limb);
	// A leg lost: down on its belly, crawling (its body set's Legs Lost), in a low capsule
	const UTHEnemyAnimSet* Set = GetAnimations();
	if ((Limb == ETHLimb::LeftLeg || Limb == ETHLimb::RightLeg) && !bCrawling && Set && Set->LegsLost && !IsDead())
	{
		bCrawling = true;
		AnimationsInUse = Set->LegsLost;
		Crouch();
	}
	OnRep_SeveredLimbs();
	ForceNetUpdate();
	return true;
}

void ATHEnemyCharacter::OnRep_SeveredLimbs()
{
	// Only once the look is built (its pieces and skins); ApplyAppearance calls this again
	if (!CurrentLook)
	{
		return;
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(LimbInfo); ++Index)
	{
		const uint8 Bit = 1 << Index;
		if ((SeveredLimbs & Bit) && !(AppliedSeveredLimbs & Bit))
		{
			AppliedSeveredLimbs |= Bit;
			ApplySevered(static_cast<ETHLimb>(Index));
		}
	}
}

void ATHEnemyCharacter::ApplySevered(ETHLimb Limb)
{
	const FLimbInfo& Info = LimbInfo[static_cast<uint8>(Limb)];
	const FName Bone(Info.Bone);
	USkeletalMeshComponent* Body = GetVisibleBody();
	const FTransform Wound = Body ? Body->GetSocketTransform(Bone) : GetActorTransform();

	// Gone from the body (its physics too: no more hits on it), and so from its clothes, which follow the body's pose; the
	// skin's wound where it was
	if (Body)
	{
		Body->TermBodiesBelow(Bone);
		CollapseLimb(Body, Bone);
		ShowWound(Body, Info.GoreMask);
	}
	for (USkeletalMeshComponent* Piece : Outfit)
	{
		ShowWound(Piece, Info.GoreMask);
	}

	// Seen and heard only where someone plays
	if (GetNetMode() == NM_DedicatedServer || !Body)
	{
		return;
	}
	if (SeverEffect)
	{
		// On the stump: the parent bone, where the limb was (the limb's own bones are shrunk to nothing, and so would it be)
		UNiagaraFunctionLibrary::SpawnSystemAttached(SeverEffect, Body, Body->GetParentBone(Bone), Wound.GetLocation(),
			Wound.Rotator(), EAttachLocation::KeepWorldPosition, true);
	}
	const TArray<TObjectPtr<USoundBase>>& Gore = Limb == ETHLimb::Head && !HeadSeverSounds.IsEmpty() ? HeadSeverSounds : SeverSounds;
	if (!Gore.IsEmpty())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Gore[FMath::RandHelper(Gore.Num())], Wound.GetLocation(),
			UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this), 1.0f, 0.0f, KillSoundAttenuation);
	}

	// The piece that falls: the look's mesh for this body, posed as the limb was, then loose (its own physics)
	const FTHSeverableLimb* Entry = CurrentLook ? CurrentLook->FindLimb(Limb) : nullptr;
	USkeletalMesh* PieceMesh = Entry && !Entry->Meshes.IsEmpty() ? Entry->Meshes[CurrentBodyIndex % Entry->Meshes.Num()].Get() : nullptr;
	if (!PieceMesh)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Piece = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Body->GetComponentTransform(), Params);
	if (!Piece)
	{
		return;
	}
	USkeletalMeshComponent* Fallen = NewObject<USkeletalMeshComponent>(Piece);
	Fallen->SetSkeletalMeshAsset(PieceMesh);
	// Dressed as the body was: the piece's own materials are the pack's first variant, not the skin, head and clothes this
	// enemy picked (developer, 2026-11-15: limbs fell off in the wrong clothes)
	MatchPieceMaterials(Fallen, Body, Outfit);
	if (Entry->Physics)
	{
		Fallen->SetPhysicsAsset(Entry->Physics);
	}
	Piece->SetRootComponent(Fallen);
	Fallen->SetWorldTransform(Body->GetComponentTransform());
	Fallen->SetCollisionProfileName(TEXT("Ragdoll"));
	// Bullets go through it (no holes, no blood in mid-air), blood droplets don't land on it
	Fallen->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Ignore);
	Fallen->SetCollisionResponseToChannel(EnemyBloodDropletChannel, ECR_Ignore);
	Fallen->RegisterComponent();
	Fallen->SetSimulatePhysics(true);
	const FVector Push = SeverDirection.IsNearlyZero() ? GetActorForwardVector() * -1.0f : FVector(SeverDirection);
	for (FBodyInstance* Each : Fallen->Bodies)
	{
		if (Each && Each->BodySetup.IsValid() && Body->GetBoneIndex(Each->BodySetup->BoneName) != INDEX_NONE)
		{
			Each->SetBodyTransform(Body->GetSocketTransform(Each->BodySetup->BoneName), ETeleportType::TeleportPhysics);
			Each->SetLinearVelocity(Push * 250.0f + FVector(0.0f, 0.0f, 120.0f), false);
		}
	}
	Piece->SetLifeSpan(SeveredPieceLifeSpan);
	SeveredPieces.Add(Piece);
}

void ATHEnemyCharacter::CollapseLimb(USkeletalMeshComponent* Body, FName Top)
{
	const USkeletalMesh* Asset = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	const int32 TopIndex = Asset ? Asset->GetRefSkeleton().FindBoneIndex(Top) : INDEX_NONE;
	if (TopIndex == INDEX_NONE)
	{
		return;
	}
	// A new body (a new look): the old one's limbs go with it
	if (CollapsedBody.Get() != Body)
	{
		if (USkeletalMeshComponent* Old = CollapsedBody.Get())
		{
			Old->UnregisterOnBoneTransformsFinalizedDelegate(CollapseHandle);
		}
		CollapsedBones.Reset();
		CollapsedBody = Body;
		CollapseHandle = Body->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ATHEnemyCharacter::CollapseSeveredBones));
	}
	// Top and every bone below it (parents come before their children)
	const FReferenceSkeleton& Bones = Asset->GetRefSkeleton();
	TArray<bool> InLimb;
	InLimb.SetNumZeroed(Bones.GetNum());
	InLimb[TopIndex] = true;
	CollapsedBones.Emplace(TopIndex, TopIndex);
	for (int32 Index = TopIndex + 1; Index < Bones.GetNum(); ++Index)
	{
		const int32 Parent = Bones.GetParentIndex(Index);
		if (Parent != INDEX_NONE && InLimb[Parent])
		{
			InLimb[Index] = true;
			CollapsedBones.Emplace(Index, TopIndex);
		}
	}
	CollapseSeveredBones();
	Body->MarkRenderDynamicDataDirty();
}

void ATHEnemyCharacter::CollapseSeveredBones()
{
	USkeletalMeshComponent* Body = CollapsedBody.Get();
	if (!Body)
	{
		return;
	}
	// The pose just finalized, the one that gets drawn (and that the clothes, following this body, are drawn with). Each limb
	// bone at its top bone's joint, scaled to nothing: the limb's skin shrinks into the stump (the top keeps its place, so the
	// order doesn't matter)
	TArray<FTransform>& Pose = const_cast<TArray<FTransform>&>(Body->GetComponentSpaceTransforms());
	for (const TPair<int32, int32>& Each : CollapsedBones)
	{
		if (Pose.IsValidIndex(Each.Key) && Pose.IsValidIndex(Each.Value))
		{
			const FTransform Joint = Pose[Each.Value];
			Pose[Each.Key] = FTransform(Joint.GetRotation(), Joint.GetTranslation(), FVector::ZeroVector);
		}
	}
}

float ATHEnemyCharacter::GetSpeedScale() const
{
	// A leg hurt (half its Limb Health) and not yet off: it limps
	if (!Behavior || bCrawling || Behavior->LimbHealth <= 0.0f)
	{
		return 1.0f;
	}
	const float Worst = FMath::Max(LimbDamage[static_cast<uint8>(ETHLimb::LeftLeg)], LimbDamage[static_cast<uint8>(ETHLimb::RightLeg)]);
	return Worst >= Behavior->LimbHealth * 0.5f ? Behavior->LimpSpeedScale : 1.0f;
}

float ATHEnemyCharacter::GetAttackDamageScale() const
{
	const int32 ArmsLost = (IsLimbSevered(ETHLimb::LeftArm) ? 1 : 0) + (IsLimbSevered(ETHLimb::RightArm) ? 1 : 0);
	return Behavior ? FMath::Pow(Behavior->LostArmDamageScale, static_cast<float>(ArmsLost)) : 1.0f;
}

void ATHEnemyCharacter::StartCrawling(UTHEnemyAnimSet* CrawlSet)
{
	if (!HasAuthority() || IsDead() || !CrawlSet || bCrawling)
	{
		return;
	}
	// Off its back (or stomach) and onto its belly, crawling
	if (bLyingDown)
	{
		bLyingDown = false;
		SetBeckoning(false);
		++Action.Count;
		Action.Kind = 6;
		OnRep_Action();
	}
	bCrawling = true;
	AnimationsInUse = CrawlSet;
	Crouch();
	ForceNetUpdate();
}

float ATHEnemyCharacter::RiseFromCrawl()
{
	if (!HasAuthority() || !bCrawling)
	{
		return 0.0f;
	}
	bCrawling = false;
	AnimationsInUse = Behavior && Behavior->Animations ? Behavior->Animations.Get() : nullptr;
	UnCrouch();
	// Up as from face down: one of its own lying poses with no lying clip (held on its get-up's start: prone), else any
	const UTHEnemyAnimSet* Set = GetAnimations();
	if (!Set || Set->LyingPoses.IsEmpty())
	{
		ForceNetUpdate();
		return 0.0f;
	}
	TArray<int32> Prone;
	for (int32 Index = 0; Index < Set->LyingPoses.Num(); ++Index)
	{
		if (!Set->LyingPoses[Index].Lying)
		{
			Prone.Add(Index);
		}
	}
	const int32 Pose = Prone.IsEmpty() ? FMath::RandHelper(Set->LyingPoses.Num()) : Prone[FMath::RandHelper(Prone.Num())];
	Action.Pose = static_cast<uint8>(Pose);
	++Action.Count;
	Action.Kind = 7;
	OnRep_Action();
	ForceNetUpdate();
	return Set->LyingPoses[Pose].GetUp.GetDuration();
}

int32 ATHEnemyCharacter::GetLyingPose() const
{
	const UTHEnemyAnimSet* Set = GetAnimations();
	return Set && Set->LyingPoses.IsValidIndex(Action.Pose) ? Action.Pose : INDEX_NONE;
}

void ATHEnemyCharacter::SetBeckoning(bool bNow)
{
	if (HasAuthority() && bBeckoning != bNow)
	{
		bBeckoning = bNow;
		ForceNetUpdate();
	}
}

float ATHEnemyCharacter::GetUp()
{
	if (!HasAuthority() || !bLyingDown)
	{
		return 0.0f;
	}
	bLyingDown = false;
	SetBeckoning(false);
	++Action.Count;
	Action.Kind = 4;
	OnRep_Action();
	ForceNetUpdate();
	const UTHEnemyAnimSet* Set = GetAnimations();
	const int32 Pose = GetLyingPose();
	return !Set ? 0.0f : Pose != INDEX_NONE ? Set->LyingPoses[Pose].GetUp.GetDuration() : Set->StumbleGetUp.GetDuration();
}

void ATHEnemyCharacter::DealBlow(AActor* Target, float Damage)
{
	if (HasAuthority() && Target && !IsDead())
	{
		UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, UDamageType::StaticClass());
	}
}

void ATHEnemyCharacter::WallCrash(const FVector& Point, const FVector& Normal)
{
	if (!HasAuthority() || !Behavior)
	{
		return;
	}
	++WallCrashCount;
	const TArray<TObjectPtr<USoundBase>>& Sounds = Behavior->WallCrashSounds;
	USoundBase* Sound = Sounds.Num() > 0 ? Sounds[FMath::RandRange(0, Sounds.Num() - 1)].Get() : nullptr;
	MulticastWallCrash(Point, Normal, Behavior->WallCrashEffect, Sound, Behavior->WallCrashShakeRadius, Behavior->WallCrashShake,
		Behavior->WallCrashEffectTime, Behavior->WallCrashEffectScale);
}

void ATHEnemyCharacter::PlayCrashAt(const FVector& Point, const FVector& Normal, USoundBase* Sound)
{
	if (HasAuthority() && Behavior)
	{
		MulticastWallCrash(Point, Normal, nullptr, Sound, Behavior->WallCrashShakeRadius, Behavior->WallCrashShake);
	}
}

void ATHEnemyCharacter::MulticastWallCrash_Implementation(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, UNiagaraSystem* Effect,
	USoundBase* Sound, float ShakeRadius, float Shake, float EffectTime, float EffectScale)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (Effect)
	{
		UNiagaraComponent* Puff = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Point, FVector(Normal).Rotation(),
			FVector(FMath::Max(EffectScale, 0.01f)));
		// A looping effect: it stops spawning after EffectTime and goes once its particles are gone
		if (Puff && EffectTime > 0.0f)
		{
			FTimerHandle Stop;
			GetWorldTimerManager().SetTimer(Stop, FTimerDelegate::CreateWeakLambda(Puff, [Puff]() { Puff->Deactivate(); }), EffectTime, false);
		}
	}
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Point);
	}
	// Each player on this machine: a jolt, strongest right by it, nothing past ShakeRadius
	if (ShakeRadius <= 0.0f || Shake <= 0.0f)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ATHPlayerController* PC = Cast<ATHPlayerController>(It->Get());
		const APawn* Pawn = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
		UTHCameraModifier* View = PC ? PC->GetBodyCameraModifier() : nullptr;
		if (!Pawn || !View)
		{
			continue;
		}
		const float Near = 1.0f - FMath::Clamp(FVector::Dist(Pawn->GetActorLocation(), FVector(Point)) / ShakeRadius, 0.0f, 1.0f);
		if (Near > 0.0f)
		{
			View->AddHit(Shake * Near);
		}
	}
}

bool ATHEnemyCharacter::IsAttacking() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < AttackEndTime;
}

void ATHEnemyCharacter::OnRep_Attack()
{
	UTHEnemyAnimInstance* Anim = Cast<UTHEnemyAnimInstance>(GetMesh()->GetAnimInstance());
	const FTHEnemyAttack* Clip = GetAnimations() ? GetAnimations()->GetAttack(Attack.Index) : nullptr;
	if (Attack.Count == 0 || IsDead() || !Anim || !Clip)
	{
		return;
	}
	Anim->PlayAttack(*Clip);
}

void ATHEnemyCharacter::LandAttack()
{
	AActor* Target = AttackTarget.Get();
	// A player who stepped back or around it in time dodged the blow
	if (!Target || IsDead() || !Behavior || !IsInReach(Target, 1.4f))
	{
		return;
	}
	UGameplayStatics::ApplyDamage(Target, Behavior->AttackDamage * GetAttackDamageScale(), GetController(), this, UDamageType::StaticClass());
}

bool ATHEnemyCharacter::IsInReach(const AActor* Target, float RangeScale) const
{
	if (!Target)
	{
		return false;
	}
	const float Range = (Behavior ? Behavior->AttackRange : 80.0f) * RangeScale;
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (FMath::Abs(ToTarget.Z) > 150.0)
	{
		return false;
	}
	const double Gap = ToTarget.Size2D() - GetCapsuleComponent()->GetScaledCapsuleRadius() - Target->GetSimpleCollisionRadius();
	const double Facing = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToTarget.GetSafeNormal2D());
	return Gap <= Range && Facing > 0.5 && HasClearReach(Target);
}

bool ATHEnemyCharacter::HasClearReach(const AActor* Target, const FVector* From) const
{
	if (!Target || !GetWorld())
	{
		return false;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THEnemyReach), false, this);
	Params.AddIgnoredActor(Target);
	// What either of them carries or owns (a gun, a light; a player's guns are owned, not always attached) doesn't count
	TArray<AActor*> Carried;
	Target->GetAttachedActors(Carried, true, true);
	GetAttachedActors(Carried, false, true);
	Carried.Append(Target->Children);
	Carried.Append(Children);
	Params.AddIgnoredActors(Carried);
	const FVector Start = From ? *From : GetActorLocation();
	for (const float Up : { 40.0f, -10.0f })
	{
		if (GetWorld()->LineTraceTestByObjectType(Start + FVector(0.0f, 0.0f, Up), Target->GetActorLocation() + FVector(0.0f, 0.0f, Up), Objects, Params))
		{
			return false;
		}
	}
	return true;
}

void ATHEnemyCharacter::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector Direction = (Location - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}
	const FRotator Wanted(0.0f, Direction.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Wanted, DeltaSeconds, GetCharacterMovement()->RotationRate.Yaw));
}

bool ATHEnemyCharacter::PlayVoice(const TArray<TObjectPtr<USoundBase>>& Sounds)
{
	if (!HasAuthority() || IsDead() || !Behavior || Sounds.IsEmpty() || IsVoicePlaying())
	{
		return false;
	}
	USoundBase* Sound = Sounds[FMath::RandHelper(Sounds.Num())];
	if (!Sound)
	{
		return false;
	}
	// Busy for the sound's length (a looping or unknown one counts as a few seconds)
	const float Duration = Sound->GetDuration();
	VoiceBusyUntil = GetWorld()->GetTimeSeconds() + (Duration > 0.0f && Duration < 30.0f ? Duration : 3.0f) + 0.3f;
	MulticastVoice(Sound, Behavior->VoiceAttenuation, Behavior->VoiceVolume);
	return true;
}

void ATHEnemyCharacter::SetCalmWalk(bool bCalm)
{
	if (HasAuthority() && bCalmWalk != bCalm)
	{
		bCalmWalk = bCalm;
		ForceNetUpdate();
	}
}

void ATHEnemyCharacter::SetAggressive(bool bNow)
{
	if (HasAuthority() && bAggressive != bNow)
	{
		bAggressive = bNow;
		ForceNetUpdate();
	}
}

void ATHEnemyCharacter::SetHeadDown(float Degrees)
{
	if (HasAuthority() && !FMath::IsNearlyEqual(HeadDown, Degrees))
	{
		HeadDown = Degrees;
		ForceNetUpdate();
	}
}

bool ATHEnemyCharacter::IsVoicePlaying() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < VoiceBusyUntil;
}

void ATHEnemyCharacter::MulticastVoice_Implementation(USoundBase* Sound, USoundAttenuation* Attenuation, float Volume)
{
	LastVoice = Sound;
	if (!Sound || IsDead() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// From about the head, following the body as it moves
	UGameplayStatics::SpawnSoundAttached(Sound, GetRootComponent(), NAME_None, FVector(0.0f, 0.0f, 70.0f),
		EAttachLocation::KeepRelativeOffset, true, Volume, VoicePitch, 0.0f, Attenuation);
}

void ATHEnemyCharacter::PlaySoundHere(USoundBase* Sound)
{
	if (HasAuthority() && Sound && Behavior)
	{
		MulticastSound(Sound, Behavior->VoiceAttenuation);
	}
}

void ATHEnemyCharacter::MulticastSound_Implementation(USoundBase* Sound, USoundAttenuation* Attenuation)
{
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		// At its feet (a stomp, a scrape)
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
			1.0f, 1.0f, 0.0f, Attenuation);
	}
}

// Death

void ATHEnemyCharacter::OnRep_Death()
{
	ApplyDeath(true);
}

void ATHEnemyCharacter::ApplyDeath(bool bAsItHappens)
{
	// Before BeginPlay the body may not have physics yet (a late joiner); BeginPlay comes back here
	if (!Death.bDead || bRagdoll || !HasActorBegunPlay())
	{
		return;
	}
	BecomeRagdoll();
	if (bAsItHappens)
	{
		PlayKillSound();
	}
}

void ATHEnemyCharacter::PlayKillSound()
{
	const bool bByShot = !FVector(Death.Direction).IsNearlyZero();
	USoundBase* Sound = KillSounds.IsEmpty() ? nullptr : KillSounds[FMath::RandHelper(KillSounds.Num())].Get();
	if (!Sound || !bByShot || HeadBoneNames.Contains(Death.Bone) || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const USkeletalMeshComponent* Body = GetMesh();
	const FVector Where = !Death.Bone.IsNone() && Body->GetBoneIndex(Death.Bone) != INDEX_NONE ? Body->GetBoneLocation(Death.Bone) : GetActorLocation();
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Where, UTHAudioSettingsSubsystem::GetGunplayVolumeFor(this), 1.0f, 0.0f, KillSoundAttenuation);
}

void ATHEnemyCharacter::BecomeRagdoll()
{
	bRagdoll = true;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Movement->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The ragdoll (the hidden animated body) takes the shots from now on; the visible one only copies it
	if (VisibleBody)
	{
		VisibleBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no physics asset: it can't fall as a ragdoll"), *GetName());
		return;
	}

	// Falls against the world; players walk through it and its bones still take shots (blood) and catch droplets
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetCollisionResponseToChannel(EnemyWeaponTraceChannel, ECR_Block);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();

	if (!Death.Direction.IsNearlyZero())
	{
		const FName Bone = !Death.Bone.IsNone() && Body->GetBodyInstance(Death.Bone) ? Death.Bone : NAME_None;
		Body->AddImpulse(FVector(Death.Direction) * DeathShotSpeed, Bone, true);
	}
}

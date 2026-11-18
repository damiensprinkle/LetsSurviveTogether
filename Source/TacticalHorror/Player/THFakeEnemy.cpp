#include "Player/THFakeEnemy.h"
#include "Player/THSanityData.h"
#include "Enemies/THEnemyAnimSet.h"
#include "Enemies/THEnemyAppearance.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TacticalHorror.h"

ATHFakeEnemy::ATHFakeEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	// The enemies' meshes face +Y; the actor faces where it runs
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCastShadow(true);
	// Posed even where nothing is drawn (the headless tests), so it can be checked
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ATHFakeEnemy::Start(APawn* InVictim, const UTHSanityData* InData)
{
	Victim = InVictim;
	Data = InData;
	if (!Data || !InVictim)
	{
		Vanish(TEXT("no data"));
		return;
	}

	// A look that plays the zombie animations itself (not a retargeted MetaHuman), by weight
	TArray<const FTHEnemyLook*> Looks;
	if (const UTHEnemyAppearance* Appearance = Data->FakeEnemyLooks)
	{
		for (const FTHEnemyLook& Each : Appearance->Looks)
		{
			if (!Each.IsRetargeted() && !Each.Bodies.IsEmpty() && Each.Weight > 0.0f)
			{
				Looks.Add(&Each);
			}
		}
	}
	if (Looks.IsEmpty())
	{
		Vanish(TEXT("no look"));
		return;
	}
	const FTHEnemyLook& Look = *Looks[FMath::RandHelper(Looks.Num())];
	auto Pick = [](const auto& Array) { return Array.IsEmpty() ? nullptr : Array[FMath::RandHelper(Array.Num())].Get(); };
	auto Dress = [&Pick](USkeletalMeshComponent* Component, const TArray<FTHMaterialChoice>& Choices)
	{
		for (const FTHMaterialChoice& Choice : Choices)
		{
			const int32 Slot = Component->GetMaterialIndex(Choice.Slot);
			if (UMaterialInterface* Material = Pick(Choice.Materials); Material && Slot != INDEX_NONE)
			{
				Component->SetMaterial(Slot, Material);
			}
		}
	};
	Body->SetSkeletalMeshAsset(Pick(Look.Bodies));
	Dress(Body, Look.Materials);
	for (const FTHOutfitPart& Part : Look.Parts)
	{
		if (Part.Choices.IsEmpty())
		{
			continue;
		}
		const FTHOutfitChoice& Choice = Part.Choices[FMath::RandHelper(Part.Choices.Num())];
		for (USkeletalMesh* Mesh : Choice.Meshes)
		{
			if (!Mesh)
			{
				continue;
			}
			USkeletalMeshComponent* Piece = NewObject<USkeletalMeshComponent>(this);
			Piece->SetSkeletalMeshAsset(Mesh);
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetupAttachment(Body);
			Piece->RegisterComponent();
			Piece->SetLeaderPoseComponent(Body);
			Dress(Piece, Choice.Materials);
			Outfit.Add(Piece);
		}
	}

	// Running
	if (const UTHEnemyAnimSet* Anims = Data->FakeEnemyAnims; Anims && Anims->Run)
	{
		Body->PlayAnimation(Anims->Run, true);
		const float Rate = Data->FakeEnemySpeed / FMath::Max(Anims->RunAnimSpeed, 1.0f);
		Body->SetPlayRate(FMath::Min(Rate, Anims->MaxRunPlayRate));
	}
	const FVector Toward = (InVictim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	SetActorRotation(Toward.Rotation());

	if (USoundBase* Shout = Pick(Data->FakeEnemySounds))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Shout, GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), 1.0f, 1.0f, 0.0f, Data->FakeEnemyAttenuation);
	}
	UE_LOG(LogTacticalHorror, Log, TEXT("Fake enemy (sanity) at %.0f cm from %s, look %s"), FVector::Dist2D(GetActorLocation(), InVictim->GetActorLocation()),
		*InVictim->GetName(), *Look.Name.ToString());
}

void ATHFakeEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* Target = Victim.Get();
	if (!Target || !Data || !EndReason.IsEmpty())
	{
		return;
	}
	Age += DeltaSeconds;
	const FVector Here = GetActorLocation();
	const FVector Toward = Target->GetActorLocation() - Here;
	if (Toward.Size2D() <= Data->FakeEnemyVanishDistance)
	{
		Vanish(TEXT("reached"));
		return;
	}
	if (Age > Data->FakeEnemyLifeSpan)
	{
		Vanish(TEXT("timed out"));
		return;
	}
	const FVector Direction = Toward.GetSafeNormal2D();
	FVector Next = Here + Direction * FMath::Min(Data->FakeEnemySpeed * DeltaSeconds, Toward.Size2D());

	// Along the floor; a wall in the way and it's gone (it doesn't go round things)
	FCollisionObjectQueryParams Statics;
	Statics.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THFakeEnemy), false, this);
	if (GetWorld()->LineTraceTestByObjectType(Here + FVector(0.0f, 0.0f, 60.0f), Next + FVector(0.0f, 0.0f, 60.0f) + Direction * 30.0f, Statics, Params))
	{
		Vanish(TEXT("blocked"));
		return;
	}
	FHitResult Floor;
	if (GetWorld()->LineTraceSingleByObjectType(Floor, Next + FVector(0.0f, 0.0f, 60.0f), Next - FVector(0.0f, 0.0f, 150.0f), Statics, Params))
	{
		Next.Z = Floor.ImpactPoint.Z;
	}
	SetActorLocationAndRotation(Next, Direction.Rotation());
}

bool ATHFakeEnemy::TakeShot(const FVector& From, const FVector& Direction)
{
	if (!EndReason.IsEmpty() || !Data)
	{
		return false;
	}
	// Its body as an upright line 40 cm thick, from the shins to the top of the head
	const FVector Feet = GetActorLocation();
	const FVector Low = Feet + FVector(0.0f, 0.0f, 20.0f);
	const FVector High = Feet + FVector(0.0f, 0.0f, 175.0f);
	const FVector End = From + Direction.GetSafeNormal() * 20000.0f;
	FVector OnShot, OnBody;
	FMath::SegmentDistToSegmentSafe(From, End, Low, High, OnShot, OnBody);
	if (FVector::Dist(OnShot, OnBody) > 40.0f)
	{
		return false;
	}
	// Not through a wall
	FCollisionObjectQueryParams Statics;
	Statics.AddObjectTypesToQuery(ECC_WorldStatic);
	Statics.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THFakeEnemyShot), false, this);
	if (Victim.IsValid())
	{
		Params.AddIgnoredActor(Victim.Get());
	}
	if (GetWorld()->LineTraceTestByObjectType(From, OnShot, Statics, Params))
	{
		return false;
	}

	// Dissolves into a puff where it was hit
	if (UNiagaraSystem* Puff = Data->FakeEnemyShotEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Puff, OnBody, FRotator(90.0f, 0.0f, 0.0f), FVector(Data->FakeEnemyShotEffectScale));
		++DissolveEffectCount;
	}
	const TArray<TObjectPtr<USoundBase>>& Sounds = Data->FakeEnemyShotSounds.IsEmpty() ? Data->FakeEnemyVanishSounds : Data->FakeEnemyShotSounds;
	if (USoundBase* Sound = Sounds.IsEmpty() ? nullptr : Sounds[FMath::RandHelper(Sounds.Num())].Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, OnBody, 1.0f, 1.0f, 0.0f, Data->FakeEnemyAttenuation);
	}
	Vanish(TEXT("shot"));
	return true;
}

void ATHFakeEnemy::Vanish(const TCHAR* Reason)
{
	EndReason = Reason;
	UE_LOG(LogTacticalHorror, Log, TEXT("Fake enemy gone (%s) after %.1f s"), Reason, Age);
	if (Data && EndReason != TEXT("shot"))
	{
		if (USoundBase* Sound = Data->FakeEnemyVanishSounds.IsEmpty() ? nullptr : Data->FakeEnemyVanishSounds[FMath::RandHelper(Data->FakeEnemyVanishSounds.Num())].Get())
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
	}
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetLifeSpan(0.1f);
}

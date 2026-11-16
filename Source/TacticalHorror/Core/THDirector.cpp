#include "Core/THDirector.h"

#include "Components/BillboardComponent.h"
#include "Enemies/THEnemyAIController.h"
#include "Enemies/THEnemyBehavior.h"
#include "Enemies/THEnemyCharacter.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "Player/Components/THHealthComponent.h"
#include "Player/THCharacter.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"

static TAutoConsoleVariable<int32> CVarDirectorDebug(TEXT("th.Director.Debug"), 0,
	TEXT("1: the director's intensity and phase on screen."));

ATHSpawnPoint::ATHSpawnPoint()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
	}
#endif
	bIsEditorOnlyActor = false;
}

ATHDirector::ATHDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetReplicates(false);	// the server's alone
}

void ATHDirector::BeginPlay()
{
	Super::BeginPlay();
	CalmSince = GetWorld()->GetTimeSeconds();
}

TArray<APawn*> ATHDirector::GetPlayers() const
{
	TArray<APawn*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		const UTHHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
		if (Health && Health->IsAlive())
		{
			Players.Add(Pawn);
		}
	}
	return Players;
}

void ATHDirector::Measure(float DeltaSeconds)
{
	const TArray<APawn*> Players = GetPlayers();
	float Gain = 0.0f;
	for (APawn* Player : Players)
	{
		// Hurt since last time
		const UTHHealthComponent* Health = Player->FindComponentByClass<UTHHealthComponent>();
		const float Now = Health->GetHealth();
		if (const float* Before = LastHealth.Find(Player))
		{
			Gain += FMath::Max(*Before - Now, 0.0f) * Settings->PerDamage;
		}
		LastHealth.Add(Player, Now);
		// Shooting since last time
		const ATHCharacter* Character = Cast<ATHCharacter>(Player);
		const UTHWeaponComponent* Weapons = Character ? Character->GetWeaponComponent() : nullptr;
		const ATHWeaponBase* Weapon = Weapons ? Weapons->GetCurrentWeapon() : nullptr;
		const float Shot = Weapon ? Weapon->GetLastServerShotTime() : -1000.0f;
		if (const float* Before = LastShot.Find(Player); Before && Shot > *Before)
		{
			Gain += Settings->PerShot;
		}
		LastShot.Add(Player, Shot);
	}
	// Enemies going for someone near a player
	for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead() || !It->IsAggressive())
		{
			continue;
		}
		for (const APawn* Player : Players)
		{
			if (FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) <= Settings->NearDistance)
			{
				Gain += Settings->PerAggressiveEnemy * DeltaSeconds;
				break;
			}
		}
	}
	Intensity = FMath::Clamp(Intensity + Gain - Settings->Decay * DeltaSeconds, 0.0f, 1.0f);
}

void ATHDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !Settings)
	{
		return;
	}
	Measure(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	// The first wave counts from when it has its settings (they may be set after it starts: a test, a script)
	if (!bScheduled)
	{
		bScheduled = true;
		WaveAt = Now + FMath::FRandRange(Settings->CalmBeforeWave.X, Settings->CalmBeforeWave.Y);
	}

	// A peak: a breather (no waves; stalkers hold off)
	if (Phase != ETHDirectorPhase::Relax && Intensity >= Settings->PeakAbove)
	{
		Phase = ETHDirectorPhase::Relax;
		RelaxUntil = Now + Settings->RelaxTime;
		for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (ATHEnemyAIController* Mind = Cast<ATHEnemyAIController>(It->GetController()))
			{
				Mind->HoldOff(Settings->RelaxTime);
			}
		}
	}
	if (Phase == ETHDirectorPhase::Relax)
	{
		if (Now < RelaxUntil)
		{
			return;
		}
		Phase = ETHDirectorPhase::Active;
	}

	// Calm long enough: a wave
	if (Intensity < Settings->CalmBelow)
	{
		if (Phase != ETHDirectorPhase::Calm)
		{
			Phase = ETHDirectorPhase::Calm;
			CalmSince = Now;
			WaveAt = Now + FMath::FRandRange(Settings->CalmBeforeWave.X, Settings->CalmBeforeWave.Y);
		}
		if (Now >= WaveAt)
		{
			SpawnWave();
			WaveAt = Now + FMath::FRandRange(Settings->CalmBeforeWave.X, Settings->CalmBeforeWave.Y);
		}
	}
	else
	{
		Phase = ETHDirectorPhase::Active;
	}

	if (CVarDirectorDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.3f, FColor::Orange,
			FString::Printf(TEXT("Director: intensity %.2f, %s, next wave in %.0f s, brought in %d"), Intensity,
				*StaticEnum<ETHDirectorPhase>()->GetNameStringByValue(static_cast<int64>(Phase)), FMath::Max(WaveAt - Now, 0.0), SpawnedCount));
	}
}

ATHSpawnPoint* ATHDirector::PickSpawnPoint() const
{
	const TArray<APawn*> Players = GetPlayers();
	if (Players.IsEmpty())
	{
		return nullptr;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<ATHSpawnPoint*> Fits;
	for (TActorIterator<ATHSpawnPoint> It(GetWorld()); It; ++It)
	{
		const FVector Point = It->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
		double Nearest = TNumericLimits<double>::Max();
		bool bSeen = false;
		for (const APawn* Player : Players)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist(Player->GetActorLocation(), Point));
			FCollisionQueryParams Params(SCENE_QUERY_STAT(THDirectorSight), false, Player);
			bSeen |= !GetWorld()->LineTraceTestByObjectType(Player->GetPawnViewLocation(), Point, Objects, Params);
		}
		if (!bSeen && Nearest >= Settings->SpawnDistance.X && Nearest <= Settings->SpawnDistance.Y)
		{
			Fits.Add(*It);
		}
	}
	return Fits.IsEmpty() ? nullptr : Fits[FMath::RandHelper(Fits.Num())];
}

bool ATHDirector::SpawnWave()
{
	if (!HasAuthority() || !Settings || Settings->Waves.IsEmpty())
	{
		return false;
	}
	int32 Alive = 0;
	for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
	{
		Alive += It->IsDead() ? 0 : 1;
	}
	ATHSpawnPoint* Point = PickSpawnPoint();
	UClass* Class = Settings->EnemyClass.LoadSynchronous();
	if (Alive >= Settings->MaxAlive || !Point || !Class)
	{
		return false;
	}
	// Which wave, by weight
	float Total = 0.0f;
	for (const FTHDirectorWave& Each : Settings->Waves)
	{
		Total += FMath::Max(Each.Weight, 0.0f);
	}
	float Roll = FMath::FRandRange(0.0f, FMath::Max(Total, UE_KINDA_SMALL_NUMBER));
	const FTHDirectorWave* Wave = &Settings->Waves.Last();
	for (const FTHDirectorWave& Each : Settings->Waves)
	{
		Roll -= FMath::Max(Each.Weight, 0.0f);
		if (Roll <= 0.0f)
		{
			Wave = &Each;
			break;
		}
	}
	// Brought in round the point, facing the nearest player; hunters are told where that player is (they come looking)
	const TArray<APawn*> Players = GetPlayers();
	APawn* Nearest = nullptr;
	for (APawn* Player : Players)
	{
		if (!Nearest || FVector::Dist(Player->GetActorLocation(), Point->GetActorLocation()) < FVector::Dist(Nearest->GetActorLocation(), Point->GetActorLocation()))
		{
			Nearest = Player;
		}
	}
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const int32 Count = FMath::Clamp(FMath::RandRange(Wave->Count.X, Wave->Count.Y), 1, FMath::Max(Settings->MaxAlive - Alive, 1));
	int32 Brought = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVector Where = Point->GetActorLocation() + FVector(120.0f, 0.0f, 0.0f).RotateAngleAxis(Index * 360.0f / Count, FVector::UpVector) * (Count > 1 ? 1.0f : 0.0f);
		FNavLocation Projected;
		if (Navigation && Navigation->ProjectPointToNavigation(Where, Projected, FVector(150.0f, 150.0f, 300.0f)))
		{
			Where = Projected.Location;
		}
		const FRotator Facing = Nearest ? (Nearest->GetActorLocation() - Where).GetSafeNormal2D().Rotation() : FRotator::ZeroRotator;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ATHEnemyCharacter* Enemy = GetWorld()->SpawnActor<ATHEnemyCharacter>(Class, Where + FVector(0.0f, 0.0f, 92.0f), Facing, Params);
		if (!Enemy)
		{
			continue;
		}
		Enemy->SetBehavior(Wave->Behavior);
		if (ATHEnemyAIController* Mind = Cast<ATHEnemyAIController>(Enemy->GetController()); Mind && Nearest && Wave->Behavior
			&& Wave->Behavior->Mode == ETHEnemyBehaviorMode::Hunter)
		{
			Mind->Summon(Nearest);
		}
		++Brought;
	}
	SpawnedCount += Brought;
	return Brought > 0;
}

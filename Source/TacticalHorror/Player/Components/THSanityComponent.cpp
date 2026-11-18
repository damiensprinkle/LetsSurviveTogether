#include "Player/Components/THSanityComponent.h"
#include "Player/Components/THHealthComponent.h"
#include "Player/THCharacter.h"
#include "Player/THFakeEnemy.h"
#include "Player/THSanityData.h"
#include "Enemies/THLightingSubsystem.h"
#include "Weapons/THWeaponBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "TacticalHorror.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSanityDebug(TEXT("th.SanityDebug"), 0,
		TEXT("1: show your sanity on screen (and, on the host, what is draining or restoring it)."));
}

/** Footsteps that aren't there, coming closer a stride at a time. */
struct FTHPhantomWalk
{
	FTHHallucinationSound Sound;
	FVector Location;
	FVector Direction;
	int32 StepsLeft = 0;
};

UTHSanityComponent::UTHSanityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UTHSanityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Only its own player knows (the others don't hear it)
	DOREPLIFETIME_CONDITION(UTHSanityComponent, Sanity, COND_OwnerOnly);
}

void UTHSanityComponent::BeginPlay()
{
	Super::BeginPlay();
	SwaySeed = FMath::FRandRange(0.0f, 1000.0f);
	// A shot fired on this machine: was it at the fake enemy? (Only ever the local player's own.)
	ShotHandle = ATHWeaponBase::OnLocalShot.AddWeakLambda(this, [this](APawn* Shooter, const FVector& From, const FVector& Direction)
	{
		ATHFakeEnemy* Fake = FakeEnemy.Get();
		if (Fake && Shooter == GetOwner())
		{
			Fake->TakeShot(From, Direction);
		}
	});
}

void UTHSanityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ATHWeaponBase::OnLocalShot.Remove(ShotHandle);
	if (ATHFakeEnemy* Fake = FakeEnemy.Get())
	{
		Fake->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

const UTHSanityData* UTHSanityComponent::GetData() const
{
	if (!LoadedData)
	{
		LoadedData = Data.LoadSynchronous();
	}
	return LoadedData;
}

void UTHSanityComponent::SetSanity(float Value)
{
	Sanity = FMath::Clamp(Value, 0.0f, MaxSanity);
}

float UTHSanityComponent::Depth(float Threshold) const
{
	return Threshold <= 0.0f || Sanity >= Threshold ? 0.0f : 1.0f - Sanity / Threshold;
}

void UTHSanityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UTHSanityData* Tuning = GetData();
	if (!Tuning)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		// Four times a second is plenty
		SinceUpdate += DeltaTime;
		if (SinceUpdate >= 0.25f)
		{
			UpdateSanity(SinceUpdate, *Tuning);
			SinceUpdate = 0.0f;
		}
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		UpdateEffects(DeltaTime, *Tuning);
	}
}

void UTHSanityComponent::UpdateSanity(float DeltaTime, const UTHSanityData& Tuning)
{
	const ATHCharacter* Self = Cast<ATHCharacter>(GetOwner());
	const UTHHealthComponent* Health = Self ? Self->GetHealthComponent() : nullptr;
	if (!Self || !Health || Health->GetHealthState() != ETHHealthState::Alive)
	{
		Rate = 0.0f;
		return;
	}
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();

	// Light: a lamp in the level on them (their own light only slows the drain)
	const UTHLightingSubsystem* Lighting = World->GetSubsystem<UTHLightingSubsystem>();
	const bool bLit = Lighting && Lighting->GetLampLightOn(Self) >= UTHLightingSubsystem::LitAbove;
	bInDarkness = !bLit;
	const bool bOwnLight = Lighting && Lighting->HasOwnLightOn(Self);

	// Teammates: the nearest living one (none at all: solo, never alone)
	float Nearest = TNumericLimits<float>::Max();
	bool bAnyTeammate = false;
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			const ATHCharacter* Other = Player ? Cast<ATHCharacter>(Player->GetPawn()) : nullptr;
			if (!Other || Other == Self || !Other->GetHealthComponent() || Other->GetHealthComponent()->GetHealthState() != ETHHealthState::Alive)
			{
				continue;
			}
			bAnyTeammate = true;
			Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(Other->GetActorLocation(), Self->GetActorLocation())));
		}
	}
	bAlone = bAnyTeammate && Nearest > Tuning.AloneDistance;
	bNearTeammate = bAnyTeammate && Nearest <= Tuning.TeammateDistance;

	const float Drain = (bInDarkness ? Tuning.DarknessDrain * (bOwnLight ? Tuning.OwnLightDarknessScale : 1.0f) : 0.0f) + (bAlone ? Tuning.AloneDrain : 0.0f);
	if (Drain > 0.0f)
	{
		LastDrainTime = Now;
	}
	const float Restore = (bLit ? Tuning.LitRestore : 0.0f) + (bNearTeammate ? Tuning.TeammateRestore : 0.0f)
		+ (Now - LastDrainTime >= Tuning.CalmDelay ? Tuning.CalmRestore : 0.0f);
	Rate = Restore - Drain;
	SetSanity(Sanity + Rate * DeltaTime);
}

void UTHSanityComponent::UpdateEffects(float DeltaTime, const UTHSanityData& Tuning)
{
	const ATHCharacter* Self = Cast<ATHCharacter>(GetOwner());
	const UTHHealthComponent* Health = Self ? Self->GetHealthComponent() : nullptr;
	const bool bAlive = Health && Health->GetHealthState() == ETHHealthState::Alive;
	const double Now = GetWorld()->GetTimeSeconds();

	if (CVarSanityDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.0f, FColor::Cyan, GetOwner()->HasAuthority()
			? FString::Printf(TEXT("Sanity %.0f (%+.2f/s)%s%s%s"), Sanity, Rate, bInDarkness ? TEXT(" dark") : TEXT(" lit"),
				bAlone ? TEXT(" alone") : TEXT(""), bNearTeammate ? TEXT(" near a teammate") : TEXT(""))
			: FString::Printf(TEXT("Sanity %.0f"), Sanity));
	}
	UpdateSway(DeltaTime, Tuning, bAlive);

	// Blacking out: the view's edges close in (eased, so a jump in sanity fades in), darker for a moment on each beat
	BlackoutDepth = FMath::FInterpTo(BlackoutDepth, bAlive ? Depth(Tuning.BlackoutBelow) : 0.0f, DeltaTime, 1.5f);
	const float BeatSwell = FMath::Exp(-static_cast<float>(Now - LastBeatTime) * 5.0f);
	const float Darker = (Tuning.BlackoutVignette.Y - Tuning.BlackoutVignette.X) * BlackoutDepth;
	BlackoutVignette = Tuning.BlackoutVignette.X + Darker * (1.0f + Tuning.BlackoutPulse * BeatSwell);
	BlackoutWeight = FMath::Clamp(BlackoutDepth * 5.0f, 0.0f, 1.0f);
	if (!bAlive)
	{
		return;
	}

	// Heartbeat: faster and louder the lower it goes
	if (const float Heart = Depth(Tuning.HeartbeatBelow); Heart > 0.0f && !Tuning.HeartbeatSounds.IsEmpty())
	{
		if (Now >= NextBeatTime)
		{
			if (USoundBase* Beat = Tuning.HeartbeatSounds[FMath::RandHelper(Tuning.HeartbeatSounds.Num())].Get())
			{
				UGameplayStatics::PlaySound2D(this, Beat, FMath::Lerp(Tuning.HeartbeatVolume.X, Tuning.HeartbeatVolume.Y, Heart));
			}
			++HeartbeatCount;
			LastBeatTime = Now;
			NextBeatTime = Now + 60.0 / FMath::Max(1.0f, FMath::Lerp(Tuning.BeatsPerMinute.X, Tuning.BeatsPerMinute.Y, Heart));
		}
	}
	else
	{
		NextBeatTime = Now;
	}

	// Sounds that aren't there, more often the lower it goes
	if (const float Mind = Depth(Tuning.HallucinationsBelow); Mind > 0.0f)
	{
		auto Schedule = [&]()
		{
			const float Interval = FMath::Lerp(Tuning.HallucinationInterval.X, Tuning.HallucinationInterval.Y, Mind);
			NextHallucinationTime = Now + Interval * FMath::FRandRange(0.7f, 1.3f);
		};
		if (NextHallucinationTime < 0.0)
		{
			Schedule();
		}
		else if (Now >= NextHallucinationTime)
		{
			PlayHallucination();
			Schedule();
		}
	}
	else
	{
		NextHallucinationTime = -1.0;
	}

	// At the bottom: something comes for them
	if (Sanity <= Tuning.FakeEnemyBelow)
	{
		if (NextFakeEnemyTime < 0.0)
		{
			NextFakeEnemyTime = Now + Tuning.FakeEnemyDelay;
		}
		else if (Now >= NextFakeEnemyTime && !FakeEnemy.IsValid())
		{
			// No room in sight for one: try again shortly
			NextFakeEnemyTime = Now + (SendFakeEnemy() ? FMath::FRandRange(Tuning.FakeEnemyInterval.X, Tuning.FakeEnemyInterval.Y) : 3.0);
		}
	}
	else
	{
		NextFakeEnemyTime = -1.0;
	}
}

void UTHSanityComponent::UpdateSway(float DeltaTime, const UTHSanityData& Tuning, bool bActive)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}
	// The view wanders on its own (smooth noise), pulled back to rest when sanity comes back; only the change since last
	// frame is applied, so the player's own aiming isn't fought
	SwayTime += DeltaTime;
	const float Amount = bActive ? Tuning.MaxSway * Depth(Tuning.SwayBelow) : 0.0f;
	const float T = SwayTime * Tuning.SwaySpeed;
	const FVector2D Target(FMath::PerlinNoise1D(T + SwaySeed) * Amount, FMath::PerlinNoise1D(T * 0.8f + SwaySeed + 57.0f) * Amount * 1.3f);
	// Eased, so it never jumps when the amount changes
	const FVector2D Next = FMath::Vector2DInterpTo(AppliedSway, Target, DeltaTime, 4.0f);
	const FVector2D Delta = Next - AppliedSway;
	if (!Delta.IsNearlyZero(1.0e-4))
	{
		FRotator View = Controller->GetControlRotation();
		View.Pitch += Delta.X;
		View.Yaw += Delta.Y;
		Controller->SetControlRotation(View);
	}
	AppliedSway = Next;
}

bool UTHSanityComponent::PlayHallucination(FName Name)
{
	const UTHSanityData* Tuning = GetData();
	const ACharacter* Self = Cast<ACharacter>(GetOwner());
	if (!Tuning || !Self)
	{
		return false;
	}
	// Those heard at this sanity (or the one asked for), not the last one again if there's another
	TArray<const FTHHallucinationSound*> Choices;
	float Total = 0.0f;
	for (const FTHHallucinationSound& Each : Tuning->Hallucinations)
	{
		const bool bWanted = Name.IsNone() ? Sanity <= Each.BelowSanity && Each.Weight > 0.0f : Each.Name == Name;
		if (bWanted && !Each.Sounds.IsEmpty())
		{
			Choices.Add(&Each);
		}
	}
	if (Choices.Num() > 1)
	{
		Choices.RemoveAll([this](const FTHHallucinationSound* Each) { return Each->Name == LastHallucination; });
	}
	if (Choices.IsEmpty())
	{
		return false;
	}
	for (const FTHHallucinationSound* Each : Choices)
	{
		Total += FMath::Max(Each->Weight, 0.01f);
	}
	float Roll = FMath::FRandRange(0.0f, Total);
	const FTHHallucinationSound* Sound = Choices.Last();
	for (const FTHHallucinationSound* Each : Choices)
	{
		Roll -= FMath::Max(Each->Weight, 0.01f);
		if (Roll <= 0.0f)
		{
			Sound = Each;
			break;
		}
	}

	// Where: behind them (out of view) or anywhere around, at its distance, in the open. A sound behind a wall or under the
	// floor is all but silenced by occlusion, so the line from their head must be clear; the most open of a few tries is
	// taken, cut short of what's in the way. Footsteps go on the floor there, voices at a head's height.
	FRotator View = Self->GetControlRotation();
	View.Pitch = 0.0f;
	View.Roll = 0.0f;
	const float HalfHeight = Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Head = Self->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight * 0.6f);
	FCollisionObjectQueryParams Statics;
	Statics.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THHallucination), false, Self);
	FVector Direction = View.Vector();
	float Distance = 0.0f;
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const float Turn = Sound->bBehind ? 180.0f + FMath::FRandRange(-60.0f, 60.0f) : FMath::FRandRange(0.0f, 360.0f);
		const FVector Each = (View + FRotator(0.0f, Turn, 0.0f)).Vector();
		const float Wanted = FMath::FRandRange(Sound->Distance.X, FMath::Max(Sound->Distance.X, Sound->Distance.Y));
		FHitResult Hit;
		const float Clear = GetWorld()->LineTraceSingleByObjectType(Hit, Head, Head + Each * Wanted, Statics, Params)
			? FMath::Max(Hit.Distance - 60.0f, 0.0f) : Wanted;
		if (Clear > Distance)
		{
			Direction = Each;
			Distance = Clear;
		}
		if (Clear >= Wanted)
		{
			break;
		}
	}
	Distance = FMath::Max(Distance, 150.0f);
	const bool bSteps = Sound->Steps.Y > 1;
	FVector Where = Head + Direction * Distance;
	if (bSteps)
	{
		FHitResult Floor;
		Where.Z = GetWorld()->LineTraceSingleByObjectType(Floor, Where, Where - FVector(0.0f, 0.0f, HalfHeight * 3.0f), Statics, Params)
			? Floor.ImpactPoint.Z + 10.0f : Self->GetActorLocation().Z - HalfHeight + 10.0f;
	}

	LastHallucination = Sound->Name;
	++HallucinationCount;
	UE_LOG(LogTacticalHorror, Log, TEXT("Sanity %.0f: hallucination %s %.0f cm away"), Sanity, *Sound->Name.ToString(), Distance);
	TSharedRef<FTHPhantomWalk> Walk = MakeShared<FTHPhantomWalk>();
	Walk->Sound = *Sound;
	Walk->Location = Where;
	Walk->Direction = -Direction;
	Walk->StepsLeft = bSteps ? FMath::RandRange(FMath::Max(Sound->Steps.X, 1), Sound->Steps.Y) : 1;
	PlayStep(Walk);
	return true;
}

void UTHSanityComponent::PlayStep(TSharedRef<FTHPhantomWalk> Walk)
{
	const FTHHallucinationSound& Sound = Walk->Sound;
	if (USoundBase* Each = Sound.Sounds[FMath::RandHelper(Sound.Sounds.Num())].Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Each, Walk->Location, Sound.Volume, 1.0f, 0.0f, Sound.Attenuation);
	}
	// The next stride closer; they stop short of the player
	--Walk->StepsLeft;
	const APawn* Self = Cast<APawn>(GetOwner());
	Walk->Location += Walk->Direction * Sound.StepLength;
	if (Walk->StepsLeft <= 0 || !Self || FVector::Dist2D(Walk->Location, Self->GetActorLocation()) < 150.0f)
	{
		return;
	}
	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Walk]() { PlayStep(Walk); }),
		Sound.StepInterval * FMath::FRandRange(0.9f, 1.1f), false);
}

bool UTHSanityComponent::SendFakeEnemy()
{
	const UTHSanityData* Tuning = GetData();
	APawn* Self = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Tuning || !Self || FakeEnemy.IsValid())
	{
		return false;
	}
	// A spot in front of them, in plain sight, on the floor, with a clear run to them
	FVector Eyes;
	FRotator View;
	if (const AController* Controller = Self->GetController())
	{
		Controller->GetPlayerViewPoint(Eyes, View);
	}
	else
	{
		Self->GetActorEyesViewPoint(Eyes, View);
	}
	View.Pitch = 0.0f;
	const float HalfHeight = Cast<ACharacter>(Self) ? Cast<ACharacter>(Self)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Feet = Self->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	FCollisionObjectQueryParams Statics;
	Statics.AddObjectTypesToQuery(ECC_WorldStatic);
	Statics.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THFakeEnemySpot), false, Self);
	for (int32 Try = 0; Try < 16; ++Try)
	{
		const float Turn = FMath::FRandRange(-Tuning->FakeEnemyAngle, Tuning->FakeEnemyAngle);
		const FVector Direction = (View + FRotator(0.0f, Turn, 0.0f)).Vector();
		const float Distance = FMath::FRandRange(Tuning->FakeEnemyDistance.X, FMath::Max(Tuning->FakeEnemyDistance.X, Tuning->FakeEnemyDistance.Y));
		const FVector Guess = Feet + Direction * Distance;
		FHitResult Floor;
		if (!World->LineTraceSingleByObjectType(Floor, Guess + FVector(0.0f, 0.0f, 100.0f), Guess - FVector(0.0f, 0.0f, 200.0f), Statics, Params))
		{
			continue;
		}
		const FVector Spot = Floor.ImpactPoint;
		// Seen from the eyes, and a clear way at knee and chest height
		if (World->LineTraceTestByObjectType(Eyes, Spot + FVector(0.0f, 0.0f, 120.0f), Statics, Params)
			|| World->LineTraceTestByObjectType(Spot + FVector(0.0f, 0.0f, 60.0f), Feet + FVector(0.0f, 0.0f, 60.0f), Statics, Params)
			|| World->LineTraceTestByObjectType(Spot + FVector(0.0f, 0.0f, 130.0f), Feet + FVector(0.0f, 0.0f, 130.0f), Statics, Params))
		{
			continue;
		}
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATHFakeEnemy* Fake = World->SpawnActor<ATHFakeEnemy>(ATHFakeEnemy::StaticClass(), Spot, (Feet - Spot).GetSafeNormal2D().Rotation(), Spawn);
		if (!Fake)
		{
			return false;
		}
		Fake->Start(Self, Tuning);
		FakeEnemy = Fake;
		++FakeEnemyCount;
		return true;
	}
	return false;
}

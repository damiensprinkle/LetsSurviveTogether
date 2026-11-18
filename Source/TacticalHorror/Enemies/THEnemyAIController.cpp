#include "Enemies/THEnemyAIController.h"

#include "Enemies/THEnemyBehavior.h"
#include "Enemies/THEnemyCharacter.h"
#include "Enemies/THEnemyAnimSet.h"
#include "EngineUtils.h"
#include "Components/SpotLightComponent.h"
#include "NavigationPath.h"
#include "Player/Components/THHealthComponent.h"
#include "Player/THCharacter.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Multiplayer/THDoorSyncComponent.h"
#include "Engine/OverlapResult.h"
#include "Enemies/THLightingSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

namespace
{
	const UTHHealthComponent* HealthOf(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
	}

	const FName GunshotTag(TEXT("Gunshot"));

	TAutoConsoleVariable<int32> CVarEnemyDebug(TEXT("th.EnemyDebug"), 0,
		TEXT("1: print on screen what each enemy sees and hears (on the server / host)."));
}

ATHEnemyAIController::ATHEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	SetPerceptionComponent(*Perception);
}

void ATHEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Enemy = Cast<ATHEnemyCharacter>(InPawn);
	Home = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ATHEnemyAIController::HandlePerception);
	ApplyBehavior();
}

void ATHEnemyAIController::ApplyBehavior()
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	Behavior = Body ? Body->GetBehavior() : nullptr;

	// Its own pace, blow rhythm and reaction time (developer, 2026-11-13: a group never moves in step)
	const float Spread = Behavior ? Behavior->Variation : 0.0f;
	SpeedVariation = 1.0f + FMath::FRandRange(-Spread, Spread);
	CooldownVariation = 1.0f + FMath::FRandRange(-Spread, Spread);
	ReactionDelay = Behavior ? RandomIn(Behavior->ReactionTime) : 0.0f;

	Known.Reset();
	Target = nullptr;
	bHasNoise = false;
	bPlayingDead = false;
	bPlayedDead = false;
	bEverSprung = false;
	bAmbushSprung = false;
	// Lying or standing while it waits: the placed enemy's choice (Wait Pose, or a tag), else its behaviour's, at random by
	// Lies Down Chance (developer, 2026-11-14: some ambushers standing, like before)
	bWaitsLying = false;
	if (Behavior && Body)
	{
		const bool bTagLying = Body->ActorHasTag(TEXT("Lying"));
		const bool bTagStanding = Body->ActorHasTag(TEXT("Standing"));
		bWaitsLying = Body->WaitPose == ETHWaitPose::Lying ? true
			: Body->WaitPose == ETHWaitPose::Standing ? false
			: bTagLying ? true
			: bTagStanding ? false
			: Behavior->bLiesDown && FMath::FRand() < Behavior->LiesDownChance;
	}
	bStriking = false;
	NextStrikeTime = 0.0;
	bHooked = false;
	Quarry = nullptr;
	bCalledIn = false;
	bWasChasing = false;
	NextIdleVoice = Now() + (Behavior ? RandomIn(Behavior->IdleSoundInterval) : 0.0f);
	StopMovement();
	NoiseMaker = nullptr;
	if (ATHEnemyCharacter* Owned = Enemy.Get())
	{
		Owned->SetHeadDown(0.0f);
		// The behaviour's own health (ambushers: one headshot), before it has been hurt
		if (Behavior && Behavior->Health > 0.0f && Owned->GetHealthComponent())
		{
			Owned->GetHealthComponent()->SetMaxHealth(Behavior->Health);
		}
	}

	const bool bSenses = Behavior != nullptr;
	if (Behavior)
	{
		SightConfig->SightRadius = Behavior->SightRadius;
		SightConfig->LoseSightRadius = FMath::Max(Behavior->LoseSightRadius, Behavior->SightRadius);
		SightConfig->PeripheralVisionAngleDegrees = Behavior->SightHalfAngle;
		HearingConfig->HearingRange = Behavior->HearingRange;
		Perception->ConfigureSense(*SightConfig);
		Perception->ConfigureSense(*HearingConfig);
	}
	Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), bSenses);
	Perception->SetSenseEnabled(UAISense_Hearing::StaticClass(), bSenses);
	Perception->RequestStimuliListenerUpdate();

	SetState(!Behavior ? ETHEnemyState::Idle
		: Behavior->Mode == ETHEnemyBehaviorMode::Ambusher ? ETHEnemyState::Ambush
		: Behavior->Mode == ETHEnemyBehaviorMode::Stalker ? ETHEnemyState::Lurk
		: Behavior->Mode == ETHEnemyBehaviorMode::Siren ? ETHEnemyState::Siren
		: ETHEnemyState::Idle);
	NextWanderTime = Now() + (Behavior ? RandomIn(Behavior->WanderPause) : 0.0f);
	if (Body)
	{
		Body->GetCharacterMovement()->MaxWalkSpeed = Behavior ? Behavior->WalkSpeed : 150.0f;
	}
}

// Senses

void ATHEnemyAIController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	APawn* Player = Cast<APawn>(Actor);
	if (!Behavior || !Player || !Player->IsPlayerControlled())
	{
		return;
	}

	const bool bSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
	if (CVarEnemyDebug.GetValueOnGameThread() && GEngine && GetPawn() && (Stimulus.WasSuccessfullySensed() || bSight))
	{
		const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), Stimulus.StimulusLocation) / 100.0f;
		const FString Mode = StaticEnum<ETHEnemyBehaviorMode>()->GetNameStringByValue(static_cast<int64>(Behavior->Mode));
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, bSight ? FColor::Yellow : FColor::Cyan, bSight
			? FString::Printf(TEXT("%s %s: %s %s at %.1f m"), *Mode, *GetPawn()->GetName(), Stimulus.WasSuccessfullySensed() ? TEXT("SEES") : TEXT("lost sight of"), *Player->GetName(), Distance)
			: FString::Printf(TEXT("%s %s: HEARD %s (loudness %.2f) at %.1f m"), *Mode, *GetPawn()->GetName(), *Stimulus.Tag.ToString(), Stimulus.Strength, Distance));
	}

	if (bSight)
	{
		FKnownPlayer& Entry = Known.FindOrAdd(Player);
		// In its view (bVisible), and made out: in the light, or close enough in the dark (IsVisible; developer, 2026-11-13)
		const bool bWasSeen = IsVisible(Player);
		Entry.bVisible = Stimulus.WasSuccessfullySensed();
		const bool bSeen = IsVisible(Player);
		// Coming into sight when it wasn't after them: a moment before it reacts (its Reaction Time)
		if (bSeen && !bWasSeen && Now() - Entry.LastSensed > Behavior->MemoryTime)
		{
			Entry.NoticeAt = Now() + ReactionDelay;
		}
		if (bSeen || bWasSeen)
		{
			Entry.Location = Stimulus.StimulusLocation;
			Entry.LastSensed = Now();
			// Out of sight: which way they were going, to search ahead of where it saw them
			Entry.Velocity = bSeen ? FVector::ZeroVector : Player->GetVelocity();
		}
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed())
	{
		// Walls between muffle it (developer, 2026-11-13: sound should be blocked by walls): each one cuts how far this
		// noise carries to Wall Muffle of it
		const float Muffled = HeardRangeThroughWalls(Stimulus.StimulusLocation, Stimulus.Strength);
		if (GetPawn() && FVector::Dist(GetPawn()->GetActorLocation(), Stimulus.StimulusLocation) > Muffled)
		{
			return;
		}
		bHasNoise = true;
		bUrgentNoise = Stimulus.Tag == GunshotTag;
		NoiseMaker = Player;
		NoiseLocation = Stimulus.StimulusLocation;
		NoiseTime = Now();
		LookAroundUntil = 0.0;
		// A stalker wakes to a sound and follows whoever made it
		if (Behavior->Mode == ETHEnemyBehaviorMode::Stalker && !bHooked)
		{
			Hook(Player);
		}
	}
}

int32 ATHEnemyAIController::CountWallsBetween(const FVector& From, const FVector& To) const
{
	// Walls, doors, crates and props (not bodies), each counted once however thick
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THEnemyHearing), false, GetPawn());
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, From, To, Objects, Params);
	TSet<const UPrimitiveComponent*> Seen;
	for (const FHitResult& Hit : Hits)
	{
		if (const UPrimitiveComponent* Component = Hit.GetComponent())
		{
			Seen.Add(Component);
		}
	}
	return Seen.Num();
}

float ATHEnemyAIController::HeardRangeThroughWalls(const FVector& Where, float Loudness) const
{
	const APawn* Self = GetPawn();
	const float Open = Behavior ? Behavior->HearingRange * FMath::Max(Loudness, 0.0f) : 0.0f;
	if (!Self || !Behavior || Behavior->WallMuffle >= 1.0f)
	{
		return Open;
	}
	// From a little above where it was made (a step is at the feet: not the floor itself) to its head
	const int32 Walls = CountWallsBetween(Where + FVector(0.0f, 0.0f, 50.0f), Self->GetPawnViewLocation());
	return Open * FMath::Pow(Behavior->WallMuffle, static_cast<float>(FMath::Min(Walls, 4)));
}

void ATHEnemyAIController::NotifyDamaged(APawn* Attacker)
{
	if (!Behavior)
	{
		return;
	}
	LastDamagedTime = Now();
	// A stagger (developer, 2026-11-13): a moment stopped, the hit reaction over the body; not mid-blow, lying, charging or down
	ATHEnemyCharacter* Body = Enemy.Get();
	if (Body && Behavior->StaggerTime > 0.0f && Now() >= NextStaggerTime && !Body->IsAttacking() && !Body->IsLyingDown()
		&& State != ETHEnemyState::Charge && State != ETHEnemyState::Roar && State != ETHEnemyState::Stumble)
	{
		// A headshot or a big hit: longer, a heavy flinch
		const float Stopped = Behavior->StaggerTime * (Body->WasLastHitHeavy() ? Behavior->HeavyStaggerScale : 1.0f);
		Body->PlayHitReaction(Body->GetLastHitSide(), Body->WasLastHitHeavy(), Stopped);
		StaggerUntil = Now() + Stopped;
		NextStaggerTime = Now() + FMath::Max(Behavior->StaggerCooldown, Behavior->StaggerTime);
		StopMovement();
	}
	// Being shot gives the shooter away
	if (Attacker && Attacker->IsPlayerControlled())
	{
		FKnownPlayer& Entry = Known.FindOrAdd(Attacker);
		Entry.Location = Attacker->GetActorLocation();
		Entry.LastSensed = Now();
		if (Behavior->Mode == ETHEnemyBehaviorMode::Stalker && !bHooked)
		{
			Hook(Attacker);
		}
	}
	// A siren turned on them and shot down to Play Dead Health: it drops and plays dead, once (developer, 2026-11-13); until
	// someone comes close or shoots it again
	const UTHHealthComponent* Health = Body ? Body->GetHealthComponent() : nullptr;
	if (Behavior->Mode == ETHEnemyBehaviorMode::Siren && bAmbushSprung && !bPlayedDead && Behavior->PlayDeadHealth > 0.0f && Health
		&& Health->IsAlive() && Health->GetHealth() <= Health->GetMaxHealth() * Behavior->PlayDeadHealth && !Body->IsLyingDown())
	{
		bPlayedDead = true;
		bPlayingDead = true;
		PlayDeadUntil = Now() + FMath::FRandRange(3.0f, 6.0f);
		bAmbushSprung = false;
		Target = nullptr;
		StopMovement();
		Body->SetAggressive(false);
		Body->LieDown(INDEX_NONE, true, true);
		return;
	}
	// A waiting ambusher shot: up for good (it doesn't lie down again), and its pack with it (developer, 2026-11-13: a
	// pack could be picked off one by one from beyond its hearing)
	if (Behavior->Mode == ETHEnemyBehaviorMode::Ambusher && !bAmbushSprung)
	{
		Spring(Attacker && Attacker->IsPlayerControlled() ? Attacker : nullptr);
		if (Body)
		{
			Body->PlayVoice(Behavior->AlertSounds);
		}
		if (Attacker && Attacker->IsPlayerControlled())
		{
			AlertPack(Attacker);
		}
	}
	bAmbushSprung = true;
}

void ATHEnemyAIController::NotifyKilled(APawn* Attacker)
{
	// Killed outright (one headshot) before it could spring: its pack still wakes, and comes for the shooter
	if (Behavior && Behavior->Mode == ETHEnemyBehaviorMode::Ambusher && Attacker && Attacker->IsPlayerControlled())
	{
		AlertPack(Attacker);
	}
}

bool ATHEnemyAIController::IsVisible(const APawn* Player) const
{
	const FKnownPlayer* Entry = Known.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Player)));
	return Entry && Entry->bVisible && CanMakeOut(Player);
}

bool ATHEnemyAIController::CanMakeOut(const APawn* Player) const
{
	// In the dark it only makes a player out within Dark Sight Distance; lit (by their own light or a lamp), as far as it sees
	// (developer, 2026-11-13: light and dark in what enemies see)
	const APawn* Self = GetPawn();
	if (!Behavior || Behavior->DarkSightDistance <= 0.0f || !Self || !Player)
	{
		return true;
	}
	if (FVector::Dist(Self->GetActorLocation(), Player->GetActorLocation()) <= Behavior->DarkSightDistance)
	{
		return true;
	}
	const UTHLightingSubsystem* Lighting = GetWorld()->GetSubsystem<UTHLightingSubsystem>();
	return !Lighting || Lighting->GetLightOn(Player) >= UTHLightingSubsystem::LitAbove;
}

FVector ATHEnemyAIController::GetPlayerLocation(const APawn* Player) const
{
	const FKnownPlayer* Entry = Known.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Player)));
	return !Entry || IsVisible(Player) ? Player->GetActorLocation() : Entry->Location;
}

bool ATHEnemyAIController::IsTargetable(const APawn* Player) const
{
	const UTHHealthComponent* Health = HealthOf(Player);
	if (!Player || !Health || Health->GetHealthState() == ETHHealthState::Dead)
	{
		return false;
	}
	return Health->IsAlive() || Behavior->bFinishOffDowned;
}

APawn* ATHEnemyAIController::PickTarget()
{
	const APawn* Self = GetPawn();
	APawn* Best = nullptr;
	double BestScore = -1.0e18;
	for (auto It = Known.CreateIterator(); It; ++It)
	{
		APawn* Player = It.Key().Get();
		FKnownPlayer& Entry = It.Value();
		if (!Player)
		{
			It.RemoveCurrent();
			continue;
		}
		// Made out now (it may have walked into the light while in view): known here, now
		const bool bSees = IsVisible(Player);
		if (bSees)
		{
			if (Now() - Entry.LastSensed > Behavior->MemoryTime)
			{
				Entry.NoticeAt = Now() + ReactionDelay;
			}
			Entry.LastSensed = Now();
			Entry.Location = Player->GetActorLocation();
			Entry.Velocity = Player->GetVelocity();
		}
		if (!IsTargetable(Player) || (!bSees && Now() - Entry.LastSensed > Behavior->MemoryTime))
		{
			continue;
		}
		// Still taking them in (Reaction Time); shot, or with them right up to it, at once
		if (Now() < Entry.NoticeAt && Now() - LastDamagedTime > 1.0 && FVector::Dist(Self->GetActorLocation(), Player->GetActorLocation()) > 300.0)
		{
			continue;
		}
		double Score = -FVector::Dist(Self->GetActorLocation(), GetPlayerLocation(Player));
		Score += bSees ? 100000.0 : 0.0;
		Score += HealthOf(Player)->IsAlive() ? 50000.0 : 0.0;
		Score += Player == Target.Get() ? 500.0 : 0.0;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Player;
		}
	}
	return Best;
}

// Thinking

void ATHEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ATHEnemyCharacter* Body = Enemy.Get();
	if (!HasAuthority() || !Body || Body->IsDead() || GetPawn() != Body)
	{
		return;
	}
	if (Behavior && Now() < StaggerUntil)
	{
		return;	// staggering from a shot: stopped, a moment
	}
	OpenDoorsAhead();
	// Stopped by a closed door: banging on it, nothing else for now
	if (BangOnDoorAhead())
	{
		return;
	}
	if (!Behavior)
	{
		SetState(ETHEnemyState::Idle);
		return;
	}

	switch (Behavior->Mode)
	{
	case ETHEnemyBehaviorMode::Hunter: ThinkHunter(DeltaSeconds); UpdateChaseVoice(); break;
	case ETHEnemyBehaviorMode::Stalker: ThinkStalker(DeltaSeconds); break;
	case ETHEnemyBehaviorMode::Ambusher: ThinkAmbusher(DeltaSeconds); UpdateChaseVoice(); break;
	case ETHEnemyBehaviorMode::Charger: ThinkCharger(DeltaSeconds); break;
	case ETHEnemyBehaviorMode::Siren: ThinkSiren(DeltaSeconds); break;
	}

	// Going for someone: the players' music speeds up (UTHAmbienceSubsystem)
	Body->SetAggressive(State == ETHEnemyState::Chase || State == ETHEnemyState::Attack || State == ETHEnemyState::Strike
		|| State == ETHEnemyState::Roar || State == ETHEnemyState::Charge);
}

void ATHEnemyAIController::UpdateChaseVoice()
{
	ATHEnemyCharacter* Body = Enemy.Get();
	const bool bChasing = State == ETHEnemyState::Chase || State == ETHEnemyState::Attack;
	if (bChasing && !bWasChasing)
	{
		// A new chase (not just a moment's break in the same one) starts with its alert
		// (An ambusher shrieks as it springs instead, and only the one that set the pack off: ThinkAmbusher)
		if (Now() - ChaseEndedTime > 3.0)
		{
			// A hunter calling others in: its scream, one players learn (developer, 2026-11-14)
			// Not one that was itself called in: it comes with its ordinary shout and doesn't pass the call on, or one scream
			// would run on through every hunter within Scream Radius of another (2026-11-14)
			const bool bScreams = !bCalledIn && Behavior->ScreamRadius > 0.0f && !Behavior->ScreamSounds.IsEmpty();
			if (Behavior->Mode != ETHEnemyBehaviorMode::Ambusher)
			{
				Body->PlayVoice(bScreams ? Behavior->ScreamSounds : Behavior->AlertSounds);
			}
			if (!bCalledIn)
			{
				CallOthers(Target.Get());
			}
		}
		NextChaseVoice = Now() + RandomIn(Behavior->ChaseSoundInterval);
	}
	else if (bChasing && Now() >= NextChaseVoice)
	{
		Body->PlayVoice(Behavior->ChaseSounds);
		NextChaseVoice = Now() + RandomIn(Behavior->ChaseSoundInterval);
	}
	else if (!bChasing && bWasChasing)
	{
		ChaseEndedTime = Now();
		bCalledIn = false;	// the next chase is its own find
	}
	// With nothing to chase, now and then its calls (a hunter calling out)
	if (!bChasing && Now() >= NextIdleVoice)
	{
		Body->PlayVoice(Behavior->IdleSounds);
		NextIdleVoice = Now() + RandomIn(Behavior->IdleSoundInterval);
	}
	bWasChasing = bChasing;
}

void ATHEnemyAIController::ThinkHunter(float DeltaSeconds)
{
	ATHEnemyCharacter* Body = Enemy.Get();

	// A flashlight on it gives the holder away, wherever it is looking
	if (Behavior->bReactsToLight)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (Player && IsTargetable(Player) && IsLitBy(Player))
			{
				FKnownPlayer& Entry = Known.FindOrAdd(Player);
				Entry.Location = Player->GetActorLocation();
				Entry.LastSensed = Now();
			}
		}
	}

	Target = PickTarget();
	if (APawn* Player = Target.Get())
	{
		Body->SetCalmWalk(false);
		LastBusyTime = Now();
		// Others after them too: the nearest comes on, the rest circle to their side or back first
		if (Flank(Player))
		{
			return;
		}
		Engage(Player, DeltaSeconds, ETHEnemyState::Chase);
		return;
	}
	if (Investigate(DeltaSeconds))
	{
		Body->SetCalmWalk(false);
		LastBusyTime = Now();
		return;
	}
	if (Behavior->PaceRadius > 0.0f)
	{
		Pace();
		return;
	}
	Body->SetCalmWalk(false);
	IdleOrWander();
}

void ATHEnemyAIController::Pace()
{
	// Round and round a circle about its spot, walking like a person; now and then it turns back the other way
	ATHEnemyCharacter* Body = Enemy.Get();
	SetState(ETHEnemyState::Wander);
	Body->SetCalmWalk(true);
	if (!HasArrived() && Now() - LastMoveRequestTime < 8.0)
	{
		return;
	}
	if (FMath::FRand() < 0.1f)
	{
		PaceDirection = -PaceDirection;
	}
	PaceStep = (PaceStep + PaceDirection + 8) % 8;
	const FVector Point = Home + FVector(Behavior->PaceRadius, 0.0f, 0.0f).RotateAngleAxis(PaceStep * 45.0f, FVector::UpVector);
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (Navigation && Navigation->ProjectPointToNavigation(Point, Projected, FVector(150.0f, 150.0f, 300.0f)))
	{
		MoveTowards(Projected.Location, Behavior->PaceSpeed, 40.0f);
	}
	else
	{
		LastMoveRequestTime = Now();	// a point off the navigation mesh (a wall): try the next one in a moment
	}
}

bool ATHEnemyAIController::IsLitBy(const APawn* Player) const
{
	// Any shining spot light the player carries (weapon light, hand flashlight): it inside the beam's cone and range,
	// nothing in between
	const APawn* Self = GetPawn();
	const FVector Chest = Self->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	TArray<AActor*> Carried;
	Player->GetAttachedActors(Carried, true, true);
	Carried.Add(const_cast<APawn*>(Player));
	for (const AActor* Holder : Carried)
	{
		TInlineComponentArray<USpotLightComponent*> Lights(Holder);
		for (const USpotLightComponent* Light : Lights)
		{
			if (!Light || !Light->IsRegistered() || Light->Intensity <= 0.0f || Holder->IsHidden())
			{
				continue;
			}
			// Really shining: the beam shown, on an item that isn't put away (a holstered gun or flashlight hides its whole
			// actor; the beam's own parents may be hidden on purpose, like the first-person arms)
			if (!Light->IsVisible() || Light->bHiddenInGame)
			{
				continue;
			}
			const FVector ToChest = Chest - Light->GetComponentLocation();
			const double Distance = ToChest.Size();
			if (Distance > Light->AttenuationRadius || Distance < 1.0)
			{
				continue;
			}
			if (FVector::DotProduct(Light->GetForwardVector(), ToChest / Distance) < FMath::Cos(FMath::DegreesToRadians(Light->OuterConeAngle)))
			{
				continue;
			}
			FCollisionQueryParams Params(SCENE_QUERY_STAT(THEnemyLit), false, Player);
			Params.AddIgnoredActors(Carried);
			Params.AddIgnoredActor(Self);
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Light->GetComponentLocation(), Chest, ECC_Visibility, Params))
			{
				return true;
			}
		}
	}
	return false;
}

void ATHEnemyAIController::ThinkCharger(float DeltaSeconds)
{
	ATHEnemyCharacter* Body = Enemy.Get();

	// Roaring: facing them, then the charge
	if (State == ETHEnemyState::Roar)
	{
		APawn* Player = ChargeTarget.Get();
		if (!Player || !IsTargetable(Player))
		{
			SetState(ETHEnemyState::Idle);
			return;
		}
		// Its warning, spread over the roar (stomps)
		const int32 Count = Behavior->ChargeTellCount;
		if (TellsPlayed < Count && !Behavior->ChargeTellSounds.IsEmpty()
			&& Now() >= PhaseEnd - Behavior->RoarTime + Behavior->RoarTime * (TellsPlayed + 1) / (Count + 1))
		{
			++TellsPlayed;
			Body->PlaySoundHere(Behavior->ChargeTellSounds[FMath::RandHelper(Behavior->ChargeTellSounds.Num())]);
		}
		Body->TurnTowards(Player->GetActorLocation(), DeltaSeconds);
		if (Now() >= PhaseEnd)
		{
			// Still a clear run? They may have stepped behind something as it roared: then it comes round instead of
			// charging into it, and roars again no sooner than Roar Retry Delay
			if (!HasClearRun(Player))
			{
				NextRoarTime = Now() + Behavior->RoarRetryDelay;
				SetState(ETHEnemyState::Chase);
				return;
			}
			ChargeDirection = (Player->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
			if (ChargeDirection.IsNearlyZero())
			{
				ChargeDirection = Body->GetActorForwardVector().GetSafeNormal2D();
			}
			ChargeStart = Body->GetActorLocation();
			ChargeStartTime = Now();
			SetSpeed(Behavior->ChargeSpeed);
			SetState(ETHEnemyState::Charge);
		}
		return;
	}

	// Charging: nearly straight (it can only turn ChargeTurnRate), until it hits someone or misses
	if (State == ETHEnemyState::Charge)
	{
		const FVector Location = Body->GetActorLocation();
		APawn* Player = ChargeTarget.Get();
		// It bends towards them only along a clear line: they ducked behind a crate, it runs on past rather than curving
		// into it (a wall they stood in front of still gets it when they sidestep)
		if (Player && IsTargetable(Player) && HasClearRun(Player))
		{
			const FVector ToThem = (Player->GetActorLocation() - Location).GetSafeNormal2D();
			if (FVector::DotProduct(ToThem, ChargeDirection) > 0.0f)
			{
				const float MaxTurn = FMath::DegreesToRadians(Behavior->ChargeTurnRate) * DeltaSeconds;
				const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(ChargeDirection, ToThem), -1.0f, 1.0f));
				if (Angle > KINDA_SMALL_NUMBER)
				{
					ChargeDirection = FMath::Lerp(ChargeDirection, ToThem, FMath::Min(1.0f, MaxTurn / Angle)).GetSafeNormal2D();
				}
			}
		}
		Body->AddMovementInput(ChargeDirection, 1.0f);

		// Anyone it runs into in front of it takes the blow
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Victim = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (Victim && IsTargetable(Victim) && GapTo(Victim) < 60.0f
				&& FVector::DotProduct((Victim->GetActorLocation() - Location).GetSafeNormal2D(), ChargeDirection) > 0.3f)
			{
				Body->DealBlow(Victim, Behavior->ChargeDamage);
				EndCharge(EChargeEnd::Hit);
				return;
			}
		}
		// Into a wall (or a crate): a wall square in front crashes it at once (it would slide along it otherwise); one it
		// glanced along until it stalled crashes it too
		FHitResult Hit;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		// At the chest, the waist and the knees: crates are lower than the chest (developer, 2026-11-12: it ran into a box,
		// nothing happened)
		const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		// Unlocked doors it bursts through (opened as it comes: developer, 2026-11-13), so they don't crash it
		FCollisionQueryParams CrashQuery(SCENE_QUERY_STAT(THChargeCrash), false, Body);
		IgnoreOpenableDoors(CrashQuery);
		OpenDoorsAhead(450.0f, true, true);	// slammed open, fast (developer, 2026-11-13: it ran into the slowly opening door)
		auto TraceAhead = [&](float Reach, bool bSquareOnly)
		{
			for (const float Up : { HalfHeight * 0.4f, 0.0f, -HalfHeight * 0.5f })
			{
				const FVector From = Location + FVector(0.0f, 0.0f, Up);
				if (!GetWorld()->LineTraceSingleByObjectType(Hit, From, From + ChargeDirection * (Radius + Reach), Objects, CrashQuery))
				{
					continue;
				}
				// A loose prop (simulating physics): knocked aside, not a wall
				if (UPrimitiveComponent* Prop = Hit.GetComponent(); Prop && Prop->IsSimulatingPhysics())
				{
					Prop->AddImpulse(ChargeDirection * 700.0f + FVector(0.0f, 0.0f, 250.0f), NAME_None, true);
					continue;
				}
				if (!bSquareOnly || FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal2D(), -ChargeDirection) > 0.5f)
				{
					return true;
				}
			}
			return false;
		};
		// Its body stopped by a wall met at a slant (the traces ahead only see one nearly square on): a crash too, unless it
		// only grazed it (developer, 2026-11-17: at an angle it didn't crash, it just stopped)
		FHitResult Bump;
		if (Body->GetBlockSince(FMath::Max(ChargeStartTime, Now() - 0.1), Bump))
		{
			AActor* Other = Bump.GetActor();
			const UPrimitiveComponent* Part = Bump.GetComponent();
			const UTHDoorSyncComponent* Door = Other ? Other->FindComponentByClass<UTHDoorSyncComponent>() : nullptr;
			const bool bScenery = Other && !Cast<APawn>(Other) && !(Part && Part->IsSimulatingPhysics()) && !(Door && Door->CanBeOpened());
			const FVector Out = Bump.ImpactNormal.GetSafeNormal2D();
			const bool bWall = Bump.ImpactNormal.Z < 0.7 && !Out.IsNearlyZero();
			if (bScenery && bWall && FVector::DotProduct(Out, -ChargeDirection) >= FMath::Sin(FMath::DegreesToRadians(Behavior->WallCrashMinAngle)))
			{
				Body->WallCrash(Bump.ImpactPoint, Out);
				EndCharge(EChargeEnd::Crash);
				return;
			}
		}
		const bool bWallAhead = TraceAhead(Behavior->WallCrashReach, true);
		const bool bStalled = !bWallAhead && Now() - ChargeStartTime > 0.6 && Body->GetVelocity().Size2D() < Behavior->ChargeSpeed * 0.3f
			&& TraceAhead(80.0f, false);
		if (bWallAhead || bStalled)
		{
			Body->WallCrash(Hit.ImpactPoint, Hit.ImpactNormal);
			EndCharge(EChargeEnd::Crash);
			return;
		}
		// A miss: past them, stopped, or too far
		const bool bPast = Player && FVector::DotProduct(Player->GetActorLocation() - Location, ChargeDirection) < -150.0f;
		const bool bBlocked = Now() - ChargeStartTime > 0.6 && Body->GetVelocity().Size2D() < Behavior->ChargeSpeed * 0.3f;
		const bool bTooFar = FVector::Dist2D(Location, ChargeStart) > Behavior->ChargeMaxDistance || Now() - ChargeStartTime > 6.0;
		if (bPast || bBlocked || bTooFar)
		{
			EndCharge(EChargeEnd::Miss);
		}
		return;
	}

	// Down after a miss (or recovering after a hit)
	if (State == ETHEnemyState::Stumble)
	{
		if (Now() < PhaseEnd)
		{
			return;
		}
		SetState(ETHEnemyState::Idle);
	}

	// A player seen at a distance: roar and charge; closer, it fights like a hunter
	Target = PickTarget();
	if (APawn* Player = Target.Get())
	{
		LastBusyTime = Now();
		// Only along a clear run at them: seen through a doorway at an angle, it went straight into the frame (developer,
		// 2026-11-12). Otherwise it comes round (Engage, below) until it has one
		if (IsVisible(Player) && FVector::Dist2D(Player->GetActorLocation(), Body->GetActorLocation()) >= Behavior->ChargeMinDistance && !Body->IsAttacking()
			&& Now() >= NextRoarTime && HasClearRun(Player))
		{
			StartRoar(Player);
			return;
		}
		Engage(Player, DeltaSeconds, ETHEnemyState::Chase);
		return;
	}
	if (Investigate(DeltaSeconds))
	{
		LastBusyTime = Now();
		return;
	}
	IdleOrWander();
	// Growls now and then while it has nothing to go after
	if (Now() >= NextIdleVoice)
	{
		Body->PlayVoice(Behavior->IdleSounds);
		NextIdleVoice = Now() + RandomIn(Behavior->IdleSoundInterval);
	}
}

void ATHEnemyAIController::ThinkSiren(float DeltaSeconds)
{
	ATHEnemyCharacter* Body = Enemy.Get();

	// The trap: lying there (or standing) calling for help; only someone close sets it off (or a shot). It works on one player at
	// a time (developer, 2026-11-13: call one away from the group): with more than one in sight it lies still and quiet; for
	// someone alone it calls, beckons, and crawls towards them, "hurt". Playing dead (shot down), it is silent and still
	if (!bAmbushSprung)
	{
		SetState(ETHEnemyState::Siren);
		const FVector Chest = Body->GetActorLocation();
		APawn* Nearest = nullptr;
		double NearestAway = TNumericLimits<double>::Max();
		int32 Watching = 0;
		const double Range = FMath::Max(Behavior->SirenDistance, Behavior->BeckonDistance) * 1.5;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (!Player || !IsTargetable(Player))
			{
				continue;
			}
			const double Away = FVector::Dist(Player->GetActorLocation(), Chest);
			if (Away > Range || IsBlocked(Player->GetPawnViewLocation(), Chest))
			{
				continue;
			}
			++Watching;
			if (Away < NearestAway)
			{
				NearestAway = Away;
				Nearest = Player;
			}
			// Close enough to set it off; playing dead, not for its first few seconds (only another shot)
			if (Away <= Behavior->SirenDistance && (!bPlayingDead || Now() >= PlayDeadUntil))
			{
				Spring(Player);
				break;
			}
		}
		if (!bAmbushSprung)
		{
			const bool bWorking = !bPlayingDead && (!Behavior->bOnlyForOne || Watching <= 1);
			const bool bNear = bWorking && Nearest && NearestAway <= Behavior->BeckonDistance;
			// Crawling towards the lone player near it
			if (bNear && Behavior->CrawlAnimations)
			{
				Body->StartCrawling(Behavior->CrawlAnimations);
				MoveTowards(Nearest->GetActorLocation(), Behavior->WalkSpeed, Behavior->SirenDistance * 0.7f);
			}
			else
			{
				StopMovement();
				if (bWaitsLying && !Body->IsLyingDown() && !Body->IsCrawling())
				{
					Body->LieDown();
				}
			}
			Body->SetBeckoning(bNear && Body->IsLyingDown());
			if (bWorking && Now() >= NextIdleVoice)
			{
				Body->PlayVoice(Behavior->IdleSounds);
				NextIdleVoice = Now() + RandomIn(Behavior->IdleSoundInterval);
			}
			return;
		}
		bPlayingDead = false;
		Body->PlayVoice(Behavior->AlertSounds);
	}

	// Turned: up off the floor first (from crawling, or lying)
	if (Body->IsCrawling() && Behavior->CrawlAnimations)
	{
		StopMovement();
		SetState(ETHEnemyState::Stumble);
		PhaseEnd = Now() + Body->RiseFromCrawl();
		return;
	}
	if (Body->IsLyingDown())
	{
		SetState(ETHEnemyState::Stumble);
		PhaseEnd = Now() + Body->GetUp();
		return;
	}
	if (State == ETHEnemyState::Stumble && Now() < PhaseEnd)
	{
		return;
	}

	// Then it hunts like a hunter
	Target = PickTarget();
	if (APawn* Player = Target.Get())
	{
		LastBusyTime = Now();
		Engage(Player, DeltaSeconds, ETHEnemyState::Chase);
		return;
	}
	if (Investigate(DeltaSeconds))
	{
		return;
	}
	IdleOrWander();
}

void ATHEnemyAIController::StartRoar(APawn* Player)
{
	ATHEnemyCharacter* Body = Enemy.Get();
	ChargeTarget = Player;
	StopMovement();
	SetState(ETHEnemyState::Roar);
	TellsPlayed = 0;
	PhaseEnd = Now() + Behavior->RoarTime;
	Body->PlayRoar();
	Body->PlayVoice(Behavior->AlertSounds);
}

bool ATHEnemyAIController::HasClearRun(const APawn* Player) const
{
	// Its body's width, from it to them: walls, doors, crates and props in the way (bodies don't count)
	const ATHEnemyCharacter* Body = Enemy.Get();
	if (!Body || !Player)
	{
		return false;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THChargeRun), false, Body);
	Params.AddIgnoredActor(Player);
	IgnoreOpenableDoors(Params);
	// The same height the charge's crash checks look at, knees to chest: a sphere at the waist missed low crates, and it
	// charged straight into them (developer, 2026-11-15: it charges into objects sometimes)
	const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.9f;
	const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Low = -HalfHeight * 0.5f, High = HalfHeight * 0.4f;
	const FVector From = Body->GetActorLocation() + FVector(0.0f, 0.0f, (Low + High) * 0.5f);
	const FVector To(Player->GetActorLocation().X, Player->GetActorLocation().Y, From.Z);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, FMath::Max(Radius, (High - Low) * 0.5f));
	FHitResult Hit;
	return !GetWorld()->SweepSingleByObjectType(Hit, From, To, FQuat::Identity, Objects, Shape, Params);
}

void ATHEnemyAIController::EndCharge(EChargeEnd How)
{
	ATHEnemyCharacter* Body = Enemy.Get();
	StopMovement();
	SetSpeed(Behavior->RunSpeed);
	SetState(ETHEnemyState::Stumble);
	if (How == EChargeEnd::Hit)
	{
		PhaseEnd = Now() + Behavior->ChargeRecoverTime;
		return;
	}
	// A miss: it pulls up (slowing on its own, no stun) and turns back on them (developer, 2026-11-17: a miss shouldn't stun)
	if (How == EChargeEnd::Miss)
	{
		PhaseEnd = Now() + Behavior->ChargeMissRecoverTime;
		return;
	}
	// A wall: stunned a long while, then shakes it off (time to run, or to shoot it)
	Body->GetCharacterMovement()->StopMovementImmediately();
	const float Length = Body->PlayStumble(Behavior->StumbleDownTime);
	PhaseEnd = Now() + FMath::Max(Length, Behavior->StumbleDownTime);
}

void ATHEnemyAIController::ThinkAmbusher(float DeltaSeconds)
{
	ATHEnemyCharacter* Body = Enemy.Get();
	if (!bAmbushSprung)
	{
		// Reacts only to what is close: a player seen near it or right next to it (felt, not seen), or a sound near it
		const FVector Location = Body->GetActorLocation();
		const double Reach = Behavior->AmbushDistance;
		APawn* Noticed = nullptr;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (!Player || !IsTargetable(Player))
			{
				continue;
			}
			const double Distance = FVector::Dist(Player->GetActorLocation(), Location);
			if ((IsVisible(Player) && Distance <= Reach) || Distance <= Reach * 0.4)
			{
				Noticed = Player;
			}
		}
		APawn* Heard = NoiseMaker.Get();
		if (!Noticed && bHasNoise && Heard && IsTargetable(Heard) && FVector::Dist(NoiseLocation, Location) <= Behavior->AmbushHearingDistance)
		{
			Noticed = Heard;
		}
		if (!Noticed)
		{
			bHasNoise = false;
			SetState(ETHEnemyState::Ambush);
			// Playing dead among the bodies (developer, 2026-11-13), or standing, head down
			if (bWaitsLying && !bEverSprung)
			{
				if (!Body->IsLyingDown())
				{
					Body->LieDown();
				}
				Body->SetHeadDown(0.0f);
			}
			else
			{
				Body->SetHeadDown(Behavior->WaitingHeadDown);
			}
			return;
		}
		Spring(Noticed);
		// Its shriek; the pack it sets off springs silent (the shriek is long; developer, 2026-11-14: only one makes it)
		Body->PlayVoice(Behavior->AlertSounds);
		AlertPack(Noticed);
	}
	Body->SetHeadDown(0.0f);

	// Playing dead: up first, fast, all of the pack at once
	if (Body->IsLyingDown())
	{
		SetState(ETHEnemyState::Stumble);
		PhaseEnd = Now() + Body->GetUp();
		return;
	}
	if (State == ETHEnemyState::Stumble && Now() < PhaseEnd)
	{
		return;
	}

	// Sprung: hunts until it has had nothing to go on for its memory time, then waits again where it is
	Target = PickTarget();
	if (APawn* Player = Target.Get())
	{
		LastBusyTime = Now();
		// A flashlight on it from a way off: back into the dark a moment (developer, 2026-11-13)
		if (ShunLight(Player))
		{
			return;
		}
		// A pack surrounds rather than coming single file (Flanks)
		if (Flank(Player))
		{
			return;
		}
		Engage(Player, DeltaSeconds, ETHEnemyState::Chase);
	}
	else if (Investigate(DeltaSeconds))
	{
		LastBusyTime = Now();
	}
	else if (Now() - LastBusyTime > Behavior->MemoryTime)
	{
		bAmbushSprung = false;
		StopMovement();
		SetState(ETHEnemyState::Ambush);
	}
	else
	{
		SetState(ETHEnemyState::Idle);
	}
}

void ATHEnemyAIController::Spring(APawn* Player)
{
	// It knows who: it goes for them, not just for where a sound came from
	if (Player)
	{
		FKnownPlayer& Entry = Known.FindOrAdd(Player);
		Entry.Location = Player->GetActorLocation();
		Entry.LastSensed = Now();
	}
	bEverSprung = true;
	bAmbushSprung = true;
	LastBusyTime = Now();
}

void ATHEnemyAIController::AlertPack(APawn* Player)
{
	const APawn* Self = GetPawn();
	if (Behavior->PackAlertRadius <= 0.0f || !Self)
	{
		return;
	}
	for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
	{
		ATHEnemyAIController* Other = Cast<ATHEnemyAIController>(It->GetController());
		if (!Other || Other == this || It->IsDead() || !Other->Behavior || Other->Behavior->Mode != ETHEnemyBehaviorMode::Ambusher
			|| Other->bAmbushSprung || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > Behavior->PackAlertRadius)
		{
			continue;
		}
		Other->Spring(Player);
	}
}

void ATHEnemyAIController::Hook(APawn* Player)
{
	// Only someone still up: it lets go of a downed quarry (ThinkStalker), so hooking one again would have it hook and let
	// go every tick, never going after a teammate still standing (2026-11-14)
	if (!Player || !IsTargetable(Player) || !HealthOf(Player)->IsAlive())
	{
		return;
	}
	bHooked = true;
	Quarry = Player;
	Target = Player;
	bStriking = false;
	bPouncing = false;
	bRushing = false;
	StandoffSince = -1.0;
	bWaylaying = false;
	ShowPhase = 0;
	SeenSince = -1.0;
	QuarryHeading = FVector::ZeroVector;
	QuarryMovingSince = QuarryStillSince = -1.0;
	NextStrikeTime = Now() + RandomIn(Behavior->StrikeDelay);
	NextOpportunityTime = Now() + RandomIn(Behavior->OpportunityDelay);
	NextShowTime = Now() + RandomIn(Behavior->ShowInterval);
	NextWaylayTime = Now() + 5.0;
	NextPreyCheck = Now() + 2.0;
	NextLureTime = Now() + RandomIn(Behavior->LureInterval) * 0.5f;
	LureUntil = 0.0;
	NextIdleVoice = Now() + RandomIn(Behavior->IdleSoundInterval);
	LurkUntil = 0.0;
	ApproachDistance = Behavior->RetreatDistance;
	bShifting = false;
}

void ATHEnemyAIController::Hide(const APawn* Player, bool bSpotted)
{
	SetState(ETHEnemyState::Retreat);
	HideRepickTime = Now() + 1.5;
	bShifting = false;
	FVector Spot;
	const FVector Location = GetPawn()->GetActorLocation();
	const FVector Away = (Location - Player->GetActorLocation()).GetSafeNormal2D();
	bHideCrouched = false;
	bool bCovered = false;
	const bool bFound = FindHidingSpot(Player, Spot, bHideCrouched, bCovered);
	// Cornered (nowhere out of sight to go) with them close: it goes for them instead of running into a wall. A rush, not a
	// sneaking strike: a strike seen on its way is given up, and cornered it is always seen, so it used to turn straight
	// back and hide again (2026-11-15)
	if ((!bFound || !bCovered) && FVector::Dist2D(Location, Player->GetActorLocation()) < CorneredDistance && Behavior->bStrikes
		&& Now() - StrikeStartTime > 6.0 && Now() - LastRushTime > 6.0)	// not again straight after a strike or rush
	{
		Rush();
		return;
	}
	if (bFound || FindPlaceAwayFrom(Player, Location, Away.IsNearlyZero() ? GetPawn()->GetActorForwardVector() : Away, Behavior->StalkDistance * 0.6f, Spot))
	{
		MoveTowards(Spot, Behavior->RetreatSpeed, 60.0f, Behavior->bCrouches);
	}
	if (bSpotted && Now() >= NextSpottedVoice && Enemy.IsValid() && Enemy->PlayVoice(Behavior->SpottedSounds))
	{
		NextSpottedVoice = Now() + 6.0;
	}
}

void ATHEnemyAIController::Rush()
{
	bPouncing = true;
	bRushing = true;
	bStriking = false;
	bShifting = false;
	bWaylaying = false;
	ShowPhase = 0;
	LastRushTime = Now();
	StandoffSince = -1.0;
	SetState(ETHEnemyState::Chase);
}

void ATHEnemyAIController::ThinkStalker(float DeltaSeconds)
{
	ATHEnemyCharacter* Body = Enemy.Get();

	// Unaware: hidden and still until it hears someone (HandlePerception), is shot, someone comes right up to it, someone
	// looks at it (developer, 2026-11-12: it sat in plain view, stared at, until they were 3 m away), or it senses someone
	// within SenseDistance, through walls (developer, 2026-11-12: it should always be looking for the player)
	if (!bHooked)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
			const double Away = Player ? FVector::Dist2D(Player->GetActorLocation(), Body->GetActorLocation()) : 0.0;
			if (Player && IsTargetable(Player) && (Away <= FMath::Max(Behavior->WakeDistance, Behavior->SenseDistance)
				|| IsInViewOf(Player, GetFeetLocation(), IsLow(), false)))
			{
				// (A downed player isn't hooked: on to the next one)
				Hook(Player);
				if (bHooked)
				{
					break;
				}
			}
		}
		if (!bHooked)
		{
			SetState(ETHEnemyState::Lurk);
			if (Behavior->bCrouches)
			{
				CrouchDown();
			}
			return;
		}
	}

	// It follows one player; when they are down or gone it goes back to lying in wait
	APawn* Player = Quarry.Get();
	if (!Player || !IsTargetable(Player) || !HealthOf(Player)->IsAlive())
	{
		bHooked = false;
		Quarry = nullptr;
		Target = nullptr;
		bStriking = false;
		bPouncing = false;
		bRushing = false;
		StopMovement();
		SetState(ETHEnemyState::Lurk);
		return;
	}

	// Its prey (developer, 2026-11-13: it should treat players as prey): every couple of seconds, the straggler or the
	// most hurt of the players it senses, unless it is busy with this one (striking, pouncing, waiting ahead, showing)
	if (Behavior->bPicksPrey && Now() >= NextPreyCheck && !bStriking && !bPouncing && !bWaylaying && ShowPhase == 0)
	{
		NextPreyCheck = Now() + 2.0;
		APawn* Prey = ChoosePrey(Player);
		if (Prey && Prey != Player)
		{
			Player = Prey;
			Quarry = Prey;
			QuarryHeading = FVector::ZeroVector;
			QuarryMovingSince = QuarryStillSince = -1.0;
			NextStalkPick = 0.0;
		}
	}
	Target = Player;

	// Which way they are going (eased), and whether they have stood still a while
	{
		const FVector TheirMove = Player->GetVelocity();
		const double TheirSpeed = TheirMove.Size2D();
		if (TheirSpeed > 80.0)
		{
			const FVector Way = TheirMove.GetSafeNormal2D();
			const FVector Eased = QuarryHeading.IsNearlyZero() ? Way
				: (QuarryHeading + (Way - QuarryHeading) * (1.0f - FMath::Exp(-DeltaSeconds * 2.0f))).GetSafeNormal2D();
			QuarryHeading = Eased.IsNearlyZero() ? Way : Eased;
			QuarryMovingSince = QuarryMovingSince < 0.0 ? Now() : QuarryMovingSince;
			QuarryStillSince = -1.0;
		}
		else
		{
			QuarryMovingSince = -1.0;
			QuarryStillSince = TheirSpeed >= 30.0 ? -1.0 : QuarryStillSince < 0.0 ? Now() : QuarryStillSince;
		}
	}

	// It knows where they are (it's hunting them), but keeps out of everyone's view
	const FVector PlayerLocation = Player->GetActorLocation();
	const double Distance = FVector::Dist2D(PlayerLocation, Body->GetActorLocation());
	// Seen: the middle of its chest or head in someone's view. Glimpsed: only an edge of it (a shoulder past cover, the
	// top of its head), which sends it round its cover rather than off to hide
	const bool bSeen = IsInAnyonesView(GetFeetLocation(), IsLow(), false);
	const bool bGlimpsed = !bSeen && IsInAnyonesView(GetFeetLocation(), IsLow(), true);
	SeenSince = bSeen ? (SeenSince < 0.0 ? Now() : SeenSince) : -1.0;

	// A standoff (developer, 2026-11-15: blocking the doorway of the room it was in, it sat crouched there and never
	// attacked): close, faced by its prey, and in their view or getting nowhere, for Standoff Time: it goes for them.
	// Its own waits (a lurk's time, a lure, waiting beside their way, showing itself) don't count
	{
		const bool bOwnWait = (State == ETHEnemyState::Lurk && Now() < LurkUntil) || Now() < LureUntil || bWaylaying || ShowPhase != 0;
		const bool bHeld = Behavior->bStrikes && Behavior->StandoffTime > 0.0f && !bPouncing && !bStriking && !bOwnWait
			&& Distance < Behavior->StandoffDistance && IsFacingTowards(Player, Body->GetActorLocation(), Behavior->ViewAngle + 15.0f)
			&& (bSeen || bGlimpsed || Body->GetVelocity().Size2D() < 50.0);
		StandoffSince = bHeld ? (StandoffSince < 0.0 ? Now() : StandoffSince) : -1.0;
		if (bHeld && Now() - StandoffSince >= Behavior->StandoffTime)
		{
			Rush();
		}
	}

	// Crossing ground someone could see it on, even facing away from it: it dashes across (developer, 2026-11-13: running
	// to avoid being seen); behind cover it creeps again. Not when striking, pouncing or stepping out to be seen
	if (Now() >= NextExposureCheck)
	{
		NextExposureCheck = Now() + 0.2;
		const FVector Feet = GetFeetLocation();
		const FVector Velocity = Body->GetVelocity();
		bExposed = IsWatchedAt(Feet, IsLow()) || (Velocity.Size2D() > 50.0 && IsWatchedAt(Feet + Velocity.GetSafeNormal2D() * 150.0f, IsLow()));
	}
	if (!bStriking && !bPouncing && ShowPhase != 2 && ShowPhase != 3 && MoveBaseSpeed > 0.0f && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		SetSpeed(bExposed ? FMath::Max(MoveBaseSpeed, Behavior->ExposedSpeed) : MoveBaseSpeed);
	}

	// Approached: they walk towards it within ApproachDistance, or are really close. Only when they move towards it: it
	// arriving near them used to count, and it fled from them just standing there (developer)
	const FVector TheirVelocity = Player->GetVelocity();
	const FVector ToBody = (Body->GetActorLocation() - PlayerLocation).GetSafeNormal2D();
	const bool bComingAtIt = FVector::DotProduct(TheirVelocity, ToBody) > 60.0;
	const bool bApproached = ((Distance < ApproachDistance && bComingAtIt) || Distance < ApproachDistance * 0.55) && Now() >= LureUntil;
	const bool bHurt = Now() - LastDamagedTime < 1.0;

	// Breathing, now and then, while it keeps its distance
	if (!bStriking && Now() >= NextIdleVoice && Distance > Behavior->CloseSoundDistance * 2.0f)
	{
		Body->PlayVoice(Behavior->IdleSounds);
		NextIdleVoice = Now() + RandomIn(Behavior->IdleSoundInterval);
	}

	// Close (developer, 2026-11-13: it was too focused on fleeing; within 5 m it should just attack): it goes straight for
	// them, seen or not, instead of hiding, and keeps at them until they get away (developer, 2026-11-13: it attacked
	// once, then ran off; goofy). Not while sneaking in for a strike (that keeps its moan) unless spotted on the way
	const bool bStrikeSpotted = bStriking && bSeen && !bStrikeAttacked && !Body->IsAttacking();
	if (!bPouncing && Behavior->bStrikes && Distance <= Behavior->PounceDistance && (!bStriking || bStrikeSpotted))
	{
		bPouncing = true;
		bStriking = false;
		bShifting = false;
		bWaylaying = false;
		ShowPhase = 0;
	}
	if (bPouncing)
	{
		// A rush keeps on from further off until its first blow (then it is a pounce like any other), or they get well
		// away, or it has gone on as long as a strike may
		bRushing = bRushing && !Body->IsAttacking() && Distance <= Behavior->StandoffDistance * 1.5 && Now() - LastRushTime < Behavior->StrikeTimeout;
		if (Body->IsAttacking() || Distance <= Behavior->PounceDistance * 1.5 || bRushing)
		{
			Engage(Player, DeltaSeconds, ETHEnemyState::Chase);
			return;
		}
		// They got away: back to stalking them
		bPouncing = false;
		NextStrikeTime = Now() + RandomIn(Behavior->StrikeDelay);
		NextOpportunityTime = Now() + RandomIn(Behavior->OpportunityDelay);
		NextStalkPick = 0.0;
	}

	// Going in for a blow. Right behind them it stops and moans, the whole moan, and only then lunges: the warning, and
	// time to turn round (developer, 2026-11-11). Seen before the blow starts, it gives up and hides.
	if (bStriking)
	{
		bStrikeAttacked |= Body->IsAttacking();
		const bool bDone = (bStrikeAttacked && !Body->IsAttacking()) || Now() - StrikeStartTime > Behavior->StrikeTimeout;
		const bool bSpottedOnTheWay = bSeen && !bStrikeAttacked && !Body->IsAttacking();
		if (!bDone && !bSpottedOnTheWay)
		{
			const bool bClose = Distance <= Behavior->CloseSoundDistance;
			const bool bWarns = !Behavior->CloseSounds.IsEmpty();
			if (bWarns && bClose && !bMoaned)
			{
				// Waits for any other sound of its to end, so it never strikes without the moan
				StopMovement();
				Body->TurnTowards(PlayerLocation, DeltaSeconds);
				SetState(ETHEnemyState::Strike);
				if (!Body->IsVoicePlaying() && Body->PlayVoice(Behavior->CloseSounds))
				{
					bMoaned = true;
					StrikeStartTime = Now();	// the timeout counts from the warning
				}
				return;
			}
			if (bWarns && bMoaned && !bStrikeAttacked && Body->IsVoicePlaying())
			{
				// Moaning: still, facing them, until it ends
				StopMovement();
				Body->TurnTowards(PlayerLocation, DeltaSeconds);
				SetState(ETHEnemyState::Strike);
				return;
			}
			Engage(Player, DeltaSeconds, ETHEnemyState::Strike);
			return;
		}
		bStriking = false;
		NextStrikeTime = Now() + RandomIn(Behavior->StrikeDelay);
		NextOpportunityTime = Now() + RandomIn(Behavior->OpportunityDelay);
		// Its blow landed (or not) with them still close: it keeps at them (it used to run off to hide)
		if (Distance <= Behavior->PounceDistance)
		{
			bPouncing = true;
			Engage(Player, DeltaSeconds, ETHEnemyState::Chase);
			return;
		}
		Hide(Player, bSpottedOnTheWay);
		return;
	}

	// Showing itself on purpose (developer, 2026-11-13; GDD: a brief silhouette can frighten more than a fight): to cover
	// beside a place far off in their view, out of their sight; a step out; a moment in view, facing them; back into cover
	// fast. Hurt or rushed, or seen before it is ready, it gives up and does what it always does
	if (ShowPhase != 0)
	{
		const bool bAbort = bHurt || bApproached || (ShowPhase == 1 && bSeen) || (ShowPhase != 3 && Now() > ShowPhaseEnd) || IsStuck();
		if (bAbort)
		{
			ShowPhase = 0;
			NextShowTime = Now() + RandomIn(Behavior->ShowInterval) * 0.5f;
		}
		else if (ShowPhase == 1)
		{
			SetState(ETHEnemyState::Stalk);
			if (HasArrived())
			{
				ShowPhase = 2;
				ShowPhaseEnd = Now() + 6.0;
				MoveTowards(ShowSpot, Behavior->StalkSpeed, 30.0f, Behavior->bCrouches);
			}
			return;
		}
		else if (ShowPhase == 2)
		{
			SetState(ETHEnemyState::Stalk);
			if (HasArrived())
			{
				ShowPhase = 3;
				ShowPhaseEnd = Now() + Behavior->ShowTime;
				StopMovement();
			}
			return;
		}
		else if (ShowPhase == 3)
		{
			SetState(ETHEnemyState::Lurk);
			Body->TurnTowards(PlayerLocation, DeltaSeconds);
			if (Now() >= ShowPhaseEnd)
			{
				ShowPhase = 4;
				ShowPhaseEnd = Now() + 6.0;
				MoveTowards(ShowCover, Behavior->ExposedSpeed, 30.0f, Behavior->bCrouches);
			}
			return;
		}
		else
		{
			SetState(ETHEnemyState::Stalk);
			if (HasArrived())
			{
				ShowPhase = 0;
				NextShowTime = Now() + RandomIn(Behavior->ShowInterval);
				SetState(ETHEnemyState::Lurk);
				LurkUntil = Now() + RandomIn(Behavior->LurkTime);
			}
			return;
		}
	}

	// Hiding: once it has set off, it runs all the way there (stepping back and forth doesn't stop and start it)
	if (State == ETHEnemyState::Retreat)
	{
		if (!HasArrived())
		{
			// Pushing against something (developer, 2026-11-12: it ran into the wall by a crate and stayed there in
			// plain view): somewhere else
			if (IsStuck())
			{
				AvoidCurrentGoal();
				Hide(Player, false);
			}
			return;
		}
		// Crouches there first if it will (low cover, or it always does), then: still in view (or it found nowhere)? On
		// again, though not every moment
		const bool bLowThere = (bHideCrouched || Behavior->bCrouches) && CanCrouch();
		if (bLowThere)
		{
			CrouchDown();
		}
		if (IsInAnyonesView(GetFeetLocation(), bLowThere))
		{
			if (Now() >= HideRepickTime)
			{
				Hide(Player, false);
			}
			return;
		}
		SetState(ETHEnemyState::Lurk);
		LurkUntil = Now() + RandomIn(Behavior->LurkTime);
		// How close is too close varies each time, so it can't be read off
		ApproachDistance = Behavior->RetreatDistance * FMath::FRandRange(0.8f, 1.2f);
		return;
	}

	// Seen, shot, or they came at it: hide. Seen far off (beyond FarSightingDistance), only for a moment, it ducks behind
	// the nearest cover, fast, instead of running off (developer, 2026-11-13: it reacted more than it hunted)
	if (bSeen || bHurt || bApproached)
	{
		const bool bFarSighting = bSeen && !bHurt && !bApproached && Distance > Behavior->FarSightingDistance
			&& Now() - SeenSince < Behavior->SightingGrace;
		if (bFarSighting)
		{
			if (bShifting && !HasArrived() && !IsStuck())
			{
				return;
			}
			FVector Spot;
			if (FindCoverNearby(Spot))
			{
				bShifting = true;
				bWaylaying = false;
				SetState(ETHEnemyState::Lurk);
				MoveTowards(Spot, Behavior->ExposedSpeed, 20.0f, Behavior->bCrouches);
				return;
			}
		}
		bShifting = false;
		bWaylaying = false;
		Hide(Player, bSeen);
		return;
	}

	// Edging round its cover: until it gets there (and still unseen; seen, it hides above)
	if (bShifting)
	{
		if (!HasArrived() && !IsStuck())
		{
			return;
		}
		if (!HasArrived())
		{
			AvoidCurrentGoal();
			StopMovement();
		}
		bShifting = false;
		SetState(ETHEnemyState::Lurk);
		if (Behavior->bCrouches)
		{
			CrouchDown();
		}
	}

	// Waiting or watching with part of it showing: it creeps round the crate or doorway out of sight, a few steps;
	// nowhere near to go, it hides properly
	if (bGlimpsed && (State == ETHEnemyState::Lurk || HasArrived()) && Now() >= NextShiftTime)
	{
		NextShiftTime = Now() + 1.0;
		FVector Spot;
		if (FindCoverNearby(Spot))
		{
			bShifting = true;
			SetState(ETHEnemyState::Lurk);
			MoveTowards(Spot, Behavior->StalkSpeed, 20.0f, Behavior->bCrouches);
		}
		else
		{
			Hide(Player, true);
		}
		return;
	}

	// Gone ahead to wait beside their way: out there until they pass it (then it is behind them: an opening) or the wait
	// runs out (developer, 2026-11-13: proactive, not just following)
	if (bWaylaying)
	{
		const bool bPassed = FVector::DotProduct(GetQuarryForward(Player), WaylaySpot - PlayerLocation) < -150.0;
		if (bPassed || Now() > WaylayUntil || IsStuck())
		{
			bWaylaying = false;
			NextOpportunityTime = bPassed ? Now() : NextOpportunityTime;
		}
		else
		{
			if (HasArrived())
			{
				SetState(ETHEnemyState::Lurk);
				StopMovement();
				Body->TurnTowards(PlayerLocation, DeltaSeconds);
			}
			else
			{
				SetState(ETHEnemyState::Stalk);
			}
			return;
		}
	}

	// A strike is due: its Strike Delay is up, or they give it an opening (reloading, shooting, standing still, a straggler)
	const bool bOpening = Behavior->bStrikes && Now() >= NextOpportunityTime && HasOpening(Player);
	const bool bStrikeDue = Behavior->bStrikes && (Now() >= NextStrikeTime || bOpening);

	// Hidden: waits a while (cut short by an opening), and moves on only once they look away from it
	if (State == ETHEnemyState::Lurk)
	{
		StopMovement();
		// Waits its time, then moves on unless they are facing it with a clear line to it (its view cone and a bit);
		// facing it through a wall no longer holds it (developer, 2026-11-12: watched through the doorways it never moved;
		// it slips off round them instead); fallen well behind, it comes back regardless (developer: it ran off and never
		// came back)
		const bool bFacedByThem = IsFacingTowards(Player, Body->GetActorLocation(), Behavior->ViewAngle + 15.0f)
			&& CanBeSeenFrom(Player, GetFeetLocation(), IsLow(), true);
		const bool bFallenBehind = Distance > Behavior->StalkDistance * 1.6;
		if ((Now() < LurkUntil && !bOpening) || (bFacedByThem && !bFallenBehind))
		{
			return;
		}
	}

	// A strike, from behind, while they look away
	if (bStrikeDue && !IsFacingTowards(Player, Body->GetActorLocation(), Behavior->ViewAngle + 15.0f))
	{
		bStriking = true;
		bStrikeAttacked = false;
		bMoaned = false;
		StrikeStartTime = Now();
		Engage(Player, DeltaSeconds, ETHEnemyState::Strike);
		return;
	}

	// A lure (developer, 2026-11-13): out of their sight 8 to 20 m off, it copies a sound (a cry, steps) and waits there for
	// them to come looking; coming at it then doesn't scare it off (within Pounce Distance it goes for them)
	if (!Behavior->LureSounds.IsEmpty() && Now() >= NextLureTime && Distance > 800.0 && Distance < 2000.0
		&& !CanBeSeenFrom(Player, GetFeetLocation(), IsLow()))
	{
		NextLureTime = Now() + RandomIn(Behavior->LureInterval);
		LureUntil = Now() + 8.0;
		++LuresPlayed;
		Body->PlaySoundHere(Behavior->LureSounds[FMath::RandHelper(Behavior->LureSounds.Num())]);
		StopMovement();
		SetState(ETHEnemyState::Lurk);
		LurkUntil = LureUntil;
		return;
	}

	// Proactive (developer, 2026-11-13: it reacted more than it hunted): now and then it shows itself far off on purpose;
	// and with them walking steadily one way, it may race ahead out of their sight and wait beside their way
	if (Behavior->bShowsItself && Now() >= NextShowTime && Distance > Behavior->ShowDistance.X * 0.7f)
	{
		NextShowTime = Now() + 8.0;	// nowhere fits: tries again in a while
		if (FindShowSpots(Player, ShowSpot, ShowCover))
		{
			ShowPhase = 1;
			ShowPhaseEnd = Now() + 20.0;
			SetState(ETHEnemyState::Stalk);
			MoveTowards(ShowCover, Behavior->StalkSpeed, 40.0f, Behavior->bCrouches);
			return;
		}
	}
	if (Now() >= NextWaylayTime && QuarryMovingSince >= 0.0 && Now() - QuarryMovingSince > 2.5)
	{
		NextWaylayTime = Now() + 10.0;
		FVector Spot;
		if (FMath::FRand() < Behavior->WaylayChance && FindWaylaySpot(Player, Spot))
		{
			bWaylaying = true;
			WaylaySpot = Spot;
			WaylayUntil = Now() + 15.0;
			SetState(ETHEnemyState::Stalk);
			MoveTowards(Spot, Behavior->RetreatSpeed, 40.0f, Behavior->bCrouches);
			return;
		}
	}

	// Stalking: from one place they can't see to the next, about StalkDistance behind them; watches when there. Only
	// behind them: in front (even out of sight) it keeps working round (developer, 2026-11-12: it hid more than it stalked)
	SetState(ETHEnemyState::Stalk);
	const bool bInBand = Distance > Behavior->StalkDistance * 0.7 && Distance < Behavior->StalkDistance * 1.3;
	const bool bBehindThem = !IsFacingTowards(Player, Body->GetActorLocation(), 90.0f);
	// Settles only where it can see them, behind their back (developer, 2026-11-13: it settled in the next room, through a
	// wall, and never came back)
	if (bInBand && bBehindThem && CanBeSeenFrom(Player, GetFeetLocation(), IsLow(), false)
		&& !IsFacingTowards(Player, Body->GetActorLocation(), Behavior->ViewAngle + 45.0f))
	{
		StopMovement();
		if (Behavior->bCrouches)
		{
			CrouchDown();
		}
		Body->TurnTowards(PlayerLocation, DeltaSeconds);
		return;
	}
	if (IsStuck())
	{
		AvoidCurrentGoal();
		NextStalkPick = 0.0;
	}
	if (Now() >= NextStalkPick || HasArrived())
	{
		NextStalkPick = Now() + 2.0;
		FVector Spot;
		if (FindStalkSpot(Player, Spot))
		{
			MoveTowards(Spot, Behavior->StalkSpeed, 100.0f, Behavior->bCrouches);
		}
	}
}

void ATHEnemyAIController::Engage(APawn* Player, float DeltaSeconds, ETHEnemyState MovingState)
{
	ATHEnemyCharacter* Body = Enemy.Get();
	const FVector PlayerLocation = GetPlayerLocation(Player);

	// Mid-attack: keep turning to the target; the blow lands on its own
	if (Body->IsAttacking())
	{
		SetState(ETHEnemyState::Attack);
		Body->TurnTowards(Player->GetActorLocation(), DeltaSeconds);
		return;
	}

	// Close enough but something in between (a crate, a door frame): round it to where nothing is, or wait for a clear
	// line; it no longer swings at them across it (developer, 2026-11-13: the hunter should wait for a clear line)
	if (IsVisible(Player) && GapTo(Player) <= Behavior->AttackRange * 1.5f && !Body->HasClearReach(Player))
	{
		SetState(MovingState);
		if (Now() >= NextReachPick || HasArrived())
		{
			NextReachPick = Now() + 0.5;
			bHasReachSpot = FindReachSpot(Player, ReachSpot);
		}
		if (bHasReachSpot)
		{
			MoveTowards(ReachSpot, Behavior->RunSpeed, 20.0f);
		}
		else
		{
			StopMovement();
			Body->TurnTowards(Player->GetActorLocation(), DeltaSeconds);
		}
		return;
	}
	bHasReachSpot = false;

	// Close enough: stop, face, swing (after the cooldown)
	if (IsVisible(Player) && GapTo(Player) <= Behavior->AttackRange)
	{
		SetState(ETHEnemyState::Attack);
		StopMovement();
		Body->TurnTowards(Player->GetActorLocation(), DeltaSeconds);
		if (Body->IsInReach(Player) && Now() - Body->GetLastAttackEndTime() >= Behavior->AttackCooldown * CooldownVariation)
		{
			Body->StartAttack(Player);
		}
		return;
	}

	SetState(MovingState);
	if (IsVisible(Player))
	{
		// Aimed at: it zig-zags as it comes (developer, 2026-11-13: hunters smarter)
		FVector Weave;
		if (FindWeavePoint(Player, Weave))
		{
			MoveTowards(Weave, Behavior->RunSpeed, 40.0f);
			return;
		}
		MoveTowardsActor(Player, Behavior->RunSpeed, Behavior->AttackRange * 0.5f);
		return;
	}

	// Lost sight: go where they were heading (their last pace, Search Ahead at most), else where they were; nobody there
	// means it lost them, so it looks around there
	const FVector Search = SearchPoint(Player, PlayerLocation);
	MoveTowards(Search, Behavior->RunSpeed, 50.0f);
	if (HasArrived())
	{
		if (FKnownPlayer* Entry = Known.Find(Player))
		{
			Entry->LastSensed = -1.0e9;
		}
		bHasNoise = true;
		bUrgentNoise = false;
		NoiseLocation = Search;
		NoiseTime = Now();
		LookAroundUntil = Now() + Behavior->LookAroundTime;
	}
}

// Hunting tactics (developer, 2026-11-13: hunters smarter)

bool ATHEnemyAIController::Flank(APawn* Player)
{
	ATHEnemyCharacter* Body = Enemy.Get();
	if (!Behavior->bFlanks || !Player || !Body || Body->IsAttacking() || !IsVisible(Player))
	{
		bFlanking = false;
		return false;
	}
	// Others after the same player: is one of them nearer? Then it comes on, and this one goes round
	const FVector PlayerLocation = Player->GetActorLocation();
	const double Mine = FVector::Dist2D(Body->GetActorLocation(), PlayerLocation);
	const ATHEnemyCharacter* Lead = nullptr;
	double LeadDistance = Mine;
	int32 Nearer = 0;
	for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const ATHEnemyAIController* Other = *It != Body ? Cast<ATHEnemyAIController>(It->GetController()) : nullptr;
		if (!Other || It->IsDead() || Other->GetTarget() != Player)
		{
			continue;
		}
		const double Theirs = FVector::Dist2D(It->GetActorLocation(), PlayerLocation);
		if (Theirs < Mine)
		{
			++Nearer;
			if (Theirs < LeadDistance)
			{
				LeadDistance = Theirs;
				Lead = *It;
			}
		}
	}
	// Alone, the nearest, or close already: straight at them
	if (!Lead || Mine < Behavior->FlankDistance * 0.9)
	{
		bFlanking = false;
		return false;
	}
	// The second goes to their side (the one it is on), the rest behind them, seen along the lead's way in
	if (!bFlanking || Now() >= NextFlankPick)
	{
		NextFlankPick = Now() + 1.0;
		const FVector In = (PlayerLocation - Lead->GetActorLocation()).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, In);
		const float Which = FVector::DotProduct(Body->GetActorLocation() - PlayerLocation, Side) >= 0.0 ? 1.0f : -1.0f;
		const FVector Wanted = PlayerLocation + (Nearer == 1 ? Side * Which : In) * Behavior->FlankDistance;
		const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		FNavLocation Projected;
		if (!Navigation || !Navigation->ProjectPointToNavigation(Wanted, Projected, FVector(200.0f, 200.0f, 300.0f)))
		{
			bFlanking = false;
			return false;
		}
		FlankPoint = Projected.Location;
	}
	// There (or nearly): it closes in
	if (FVector::Dist2D(Body->GetActorLocation(), FlankPoint) < 200.0)
	{
		bFlanking = false;
		return false;
	}
	bFlanking = true;
	SetState(ETHEnemyState::Chase);
	Body->SetCalmWalk(false);
	MoveTowards(FlankPoint, Behavior->RunSpeed, 80.0f);
	return true;
}

bool ATHEnemyAIController::FindWeavePoint(const APawn* Player, FVector& OutLocation)
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	if (Behavior->WeaveOffset <= 0.0f || !Player || !Body)
	{
		return false;
	}
	const FVector Self = Body->GetActorLocation();
	const FVector ToThem = Player->GetActorLocation() - Self;
	const double Distance = ToThem.Size2D();
	// Aimed at: they face it closely, with a clear line, from 4 to 15 m
	if (Distance < 400.0 || Distance > 1500.0 || !IsFacingTowards(Player, Self, Behavior->WeaveAngle)
		|| IsBlocked(Player->GetPawnViewLocation(), Self + FVector(0.0f, 0.0f, 40.0f)))
	{
		return false;
	}
	if (Now() >= NextWeaveFlip)
	{
		WeaveSide = -WeaveSide;
		NextWeaveFlip = Now() + FMath::FRandRange(0.6f, 1.0f);
	}
	const FVector Ahead = ToThem.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead) * WeaveSide;
	const FVector Wanted = Self + Ahead * FMath::Min(Distance - 200.0, 350.0) + Side * Behavior->WeaveOffset;
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (!Navigation || !Navigation->ProjectPointToNavigation(Wanted, Projected, FVector(100.0f, 100.0f, 300.0f)))
	{
		return false;
	}
	OutLocation = Projected.Location;
	return true;
}

FVector ATHEnemyAIController::SearchPoint(const APawn* Player, const FVector& Last) const
{
	const FKnownPlayer* Entry = Known.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Player)));
	const double Pace = Entry ? Entry->Velocity.Size2D() : 0.0;
	if (!Entry || Behavior->SearchAhead <= 0.0f || Pace < 60.0)
	{
		return Last;
	}
	// Where they were going: two seconds at their pace, Search Ahead at most, if it can get there from where it saw them
	const FVector Wanted = Last + Entry->Velocity.GetSafeNormal2D() * FMath::Min(Pace * 2.0, static_cast<double>(Behavior->SearchAhead));
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (!Navigation || !Navigation->ProjectPointToNavigation(Wanted, Projected, FVector(150.0f, 150.0f, 300.0f)))
	{
		return Last;
	}
	FVector Hit;
	return UNavigationSystemV1::NavigationRaycast(GetWorld(), Last, Projected.Location, Hit) ? Hit : Projected.Location;
}

void ATHEnemyAIController::CallOthers(APawn* Player)
{
	const APawn* Self = GetPawn();
	if (!Behavior || Behavior->ScreamRadius <= 0.0f || !Player || !Self)
	{
		return;
	}
	for (TActorIterator<ATHEnemyCharacter> It(GetWorld()); It; ++It)
	{
		ATHEnemyAIController* Other = *It != Self ? Cast<ATHEnemyAIController>(It->GetController()) : nullptr;
		if (Other && !It->IsDead() && Other->Behavior && Other->Behavior->Mode == Behavior->Mode && !Other->Target.IsValid()
			&& FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) <= Behavior->ScreamRadius)
		{
			Other->HearCall(Player);
		}
	}
}

void ATHEnemyAIController::HearCall(APawn* Player)
{
	if (!Player || !IsTargetable(Player))
	{
		return;
	}
	// It knows where they are now (not seen: it runs there, and sees them or searches)
	FKnownPlayer& Entry = Known.FindOrAdd(Player);
	Entry.Location = Player->GetActorLocation();
	Entry.LastSensed = Now();
	Entry.NoticeAt = 0.0;
	bHasNoise = true;
	bUrgentNoise = true;
	NoiseLocation = Entry.Location;
	NoiseTime = Now();
	LookAroundUntil = 0.0;
	LastBusyTime = Now();
	bCalledIn = true;
}

bool ATHEnemyAIController::ShunLight(APawn* Player)
{
	ATHEnemyCharacter* Body = Enemy.Get();
	if (!Behavior->bShunsLight || !Player || !Body || Body->IsAttacking())
	{
		return false;
	}
	// Backing off already: until it is done
	if (Now() < ShunUntil)
	{
		SetState(ETHEnemyState::Retreat);
		return true;
	}
	if (Now() < NextShunTime || FVector::Dist2D(Body->GetActorLocation(), Player->GetActorLocation()) < 300.0 || !IsLitBy(Player))
	{
		return false;
	}
	// Away from the light, 4 m, out of their sight if it can
	const FVector Away = (Body->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	FVector Spot;
	if (!FindPlaceAwayFrom(Player, Body->GetActorLocation(), Away, 400.0f, Spot))
	{
		return false;
	}
	ShunUntil = Now() + 1.2;
	NextShunTime = Now() + 4.0;
	SetState(ETHEnemyState::Retreat);
	MoveTowards(Spot, Behavior->RunSpeed, 40.0f);
	return true;
}

void ATHEnemyAIController::IgnoreOpenableDoors(FCollisionQueryParams& Params) const
{
	for (TObjectIterator<UTHDoorSyncComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->GetOwner() && It->CanBeOpened())
		{
			Params.AddIgnoredActor(It->GetOwner());
		}
	}
}

void ATHEnemyAIController::OpenDoorsAhead(float Ahead, bool bAlways, bool bSlam)
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	if (!Behavior || (!Behavior->bOpensDoors && !bAlways) || !Body || (!bAlways && Now() < NextDoorCheck) || Body->GetVelocity().Size2D() < 30.0)
	{
		return;
	}
	NextDoorCheck = Now() + 0.25;
	// Only a door in its way: a body-wide sweep along where it is going, not a ball around it (developer, 2026-11-13: enemies
	// opened every door they passed, side doors too). A charge sweeps a little wider, a long way ahead.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector Start = Body->GetActorLocation();
	const FVector End = Start + Body->GetVelocity().GetSafeNormal2D() * Ahead;
	const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius() + (bSlam ? 25.0f : 10.0f);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius),
		FCollisionQueryParams(SCENE_QUERY_STAT(THEnemyDoors), false, Body));
	TSet<AActor*> Doors;
	for (const FHitResult& Each : Hits)
	{
		AActor* Hit = Each.GetActor();
		if (Hit && !Doors.Contains(Hit))
		{
			Doors.Add(Hit);
			UTHDoorSyncComponent* Door = Hit->FindComponentByClass<UTHDoorSyncComponent>();
			const bool bMoves = Door && Door->HandleRequest(const_cast<ATHEnemyCharacter*>(Body), true, bSlam);
			// Slammed: its crash at the door, once a door (developer, 2026-11-14)
			if (bMoves && bSlam && Door != LastSlammedDoor.Get() && !Behavior->DoorSlamSounds.IsEmpty())
			{
				LastSlammedDoor = Door;
				ATHEnemyCharacter* Charger = Enemy.Get();
				Charger->PlayCrashAt(Each.ImpactPoint, Each.ImpactNormal, Behavior->DoorSlamSounds[FMath::RandHelper(Behavior->DoorSlamSounds.Num())]);
			}
		}
	}
}

bool ATHEnemyAIController::BangOnDoorAhead()
{
	ATHEnemyCharacter* Body = Enemy.Get();
	if (!Behavior || !Behavior->bBangsOnDoors || !Body)
	{
		return false;
	}
	// Mid-blow: it stays at the door (the door opening ends it at once)
	UTHDoorSyncComponent* Door = BangingOn.Get();
	if (Door && Now() < BangUntil)
	{
		if (!Door->IsShut())
		{
			BangUntil = 0.0;
			BangingOn = nullptr;
			return false;
		}
		Body->TurnTowards(Door->GetOwner()->GetActorLocation(), GetWorld()->GetDeltaSeconds());
		return true;
	}
	BangingOn = nullptr;
	// Only after someone (a player, or a noise), trying to get on its way, and pressed against the door: where its path goes
	if ((!Target.IsValid() && !bHasNoise) || GetMoveStatus() != EPathFollowingStatus::Moving || Now() < NextBangCheck)
	{
		return false;
	}
	NextBangCheck = Now() + 0.2;
	FVector Way = GetPathFollowingComponent() ? GetPathFollowingComponent()->GetCurrentDirection().GetSafeNormal2D() : FVector::ZeroVector;
	if (Way.IsNearlyZero())
	{
		Way = Body->GetVelocity().GetSafeNormal2D();
	}
	if (Way.IsNearlyZero())
	{
		return false;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector Start = Body->GetActorLocation();
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, Start + Way * 60.0f, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(Body->GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.0f),
		FCollisionQueryParams(SCENE_QUERY_STAT(THEnemyDoorBang), false, Body));
	for (const FHitResult& Hit : Hits)
	{
		UTHDoorSyncComponent* Found = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UTHDoorSyncComponent>() : nullptr;
		if (!Found || !Found->IsShut())
		{
			continue;
		}
		StopMovement();
		Body->SetActorRotation(FRotator(0.0, (-Hit.ImpactNormal.GetSafeNormal2D()).Rotation().Yaw, 0.0));
		const float Swing = Body->BangOnDoor(Hit.ImpactPoint, Hit.ImpactNormal);
		BangingOn = Found;
		BangUntil = Now() + FMath::Max<double>(Swing, FMath::FRandRange(Behavior->DoorBangInterval.X, Behavior->DoorBangInterval.Y));
		return true;
	}
	return false;
}

bool ATHEnemyAIController::Investigate(float DeltaSeconds)
{
	if (!bHasNoise)
	{
		return false;
	}
	// A noise it can't get to (behind a closed or locked door it bangs on, or no way there): it gives up after a while. It
	// used to keep at it for good, banging on the door until someone opened it (2026-11-14)
	if (LookAroundUntil <= 0.0 && Behavior->InvestigateGiveUpTime > 0.0f && Now() - NoiseTime > Behavior->InvestigateGiveUpTime)
	{
		bHasNoise = false;
		StopMovement();
		NextWanderTime = Now() + RandomIn(Behavior->WanderPause);
		return false;
	}
	SetState(ETHEnemyState::Investigate);
	if (LookAroundUntil <= 0.0)
	{
		MoveTowards(NoiseLocation, bUrgentNoise ? Behavior->RunSpeed : Behavior->WalkSpeed, 80.0f);
		if (HasArrived())
		{
			LookAroundUntil = Now() + Behavior->LookAroundTime;
		}
		return true;
	}
	// Looking around: a slow turn on the spot
	if (Now() < LookAroundUntil)
	{
		APawn* Body = GetPawn();
		const FRotator Turn(0.0f, Body->GetActorRotation().Yaw + 70.0f * DeltaSeconds, 0.0f);
		Body->SetActorRotation(Turn);
		return true;
	}
	bHasNoise = false;
	LookAroundUntil = 0.0;
	NextWanderTime = Now() + RandomIn(Behavior->WanderPause);
	return false;
}

void ATHEnemyAIController::IdleOrWander()
{
	APawn* Body = GetPawn();
	if (Behavior->WanderRadius <= 0.0f)
	{
		// Back to its place if something drew it away
		if (FVector::Dist2D(Body->GetActorLocation(), Home) > 150.0)
		{
			SetState(ETHEnemyState::Wander);
			MoveTowards(Home, Behavior->WalkSpeed, 50.0f);
		}
		else
		{
			SetState(ETHEnemyState::Idle);
		}
		return;
	}

	if (State == ETHEnemyState::Wander && !HasArrived())
	{
		return;
	}
	if (State == ETHEnemyState::Wander)
	{
		NextWanderTime = Now() + RandomIn(Behavior->WanderPause);
	}
	SetState(ETHEnemyState::Idle);
	if (Now() < NextWanderTime)
	{
		return;
	}
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Spot;
	if (Navigation && Navigation->GetRandomReachablePointInRadius(Home, Behavior->WanderRadius, Spot))
	{
		SetState(ETHEnemyState::Wander);
		MoveTowards(Spot.Location, Behavior->WalkSpeed, 50.0f);
	}
	else
	{
		NextWanderTime = Now() + RandomIn(Behavior->WanderPause);
	}
}

// Moving

void ATHEnemyAIController::StopMovement()
{
	Super::StopMovement();
	// The next MoveTowards asks for a path again, even to the same place
	MoveGoalActor = nullptr;
	MoveGoal = FVector(1.0e9f);
	LastMoveRequestTime = -1.0e9;
}

void ATHEnemyAIController::SetSpeed(float Speed)
{
	if (ATHEnemyCharacter* Body = Enemy.Get())
	{
		Speed *= SpeedVariation;
		// Wounded: a leg hurt, it limps; a leg off, it crawls no faster than its crawl clip can be played (twice its pace, the
		// animation's limit), so the hands don't slide
		Speed *= Body->GetSpeedScale();
		if (Body->IsCrawling() && Body->GetAnimations())
		{
			const float ClipSpeed = Body->GetAnimations()->WalkAnimSpeed;
			// (Mixamo's Zombie Crawl, 82 cm/s, since 2026-11-14: at most about 120)
			Speed = FMath::Min(Speed, ClipSpeed > 0.0f ? ClipSpeed * 1.5f : 20.0f);
		}
		Body->GetCharacterMovement()->MaxWalkSpeed = Speed;
		Body->GetCharacterMovement()->MaxWalkSpeedCrouched = Speed;
	}
}

void ATHEnemyAIController::MoveTowards(const FVector& Location, float Speed, float AcceptanceRadius, bool bCrouched)
{
	if (bCrouched)
	{
		CrouchDown();
	}
	else
	{
		StandUp();
	}
	SetSpeed(Speed);
	MoveBaseSpeed = Speed;
	const bool bNewGoal = MoveGoalActor.IsValid() || FVector::Dist(MoveGoal, Location) > 100.0;
	if (!bNewGoal && (GetMoveStatus() != EPathFollowingStatus::Idle || Now() - LastMoveRequestTime < 0.5))
	{
		return;
	}
	if (!bNewGoal && HasArrived())
	{
		return;
	}
	MoveGoal = Location;
	MoveGoalActor = nullptr;
	LastMoveRequestTime = Now();
	SlowSince = -1.0;
	LastMoveRequest = MoveToLocation(Location, AcceptanceRadius, true, true, true, false, nullptr, true);
}

void ATHEnemyAIController::MoveTowardsActor(AActor* Goal, float Speed, float AcceptanceRadius)
{
	StandUp();
	SetSpeed(Speed);
	MoveBaseSpeed = Speed;
	if (MoveGoalActor.Get() == Goal && GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}
	if (MoveGoalActor.Get() == Goal && Now() - LastMoveRequestTime < 0.25)
	{
		return;
	}
	MoveGoalActor = Goal;
	LastMoveRequestTime = Now();
	LastMoveRequest = MoveToActor(Goal, AcceptanceRadius, true, true, false, nullptr, true);
}

bool ATHEnemyAIController::CanCrouch() const
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	return Body && Body->GetAnimations() && Body->GetAnimations()->Crouch;
}

void ATHEnemyAIController::CrouchDown()
{
	ATHEnemyCharacter* Body = Enemy.Get();
	if (Body && CanCrouch() && !Body->bIsCrouched)
	{
		Body->Crouch();
	}
}

bool ATHEnemyAIController::CoversCrouched() const
{
	return Behavior && Behavior->bUsesLowCover && Behavior->bCrouches && CanCrouch();
}

bool ATHEnemyAIController::IsLow() const
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	return Body && CanCrouch() && (Body->bIsCrouched || Body->GetCharacterMovement()->bWantsToCrouch);
}

void ATHEnemyAIController::StandUp()
{
	ATHEnemyCharacter* Body = Enemy.Get();
	// Crawling (a leg shot off): it never gets up
	if (Body && Body->IsCrawling())
	{
		return;
	}
	// A crouching stalker stays crouched whatever it does (getting up and down looked wrong; developer, 2026-11-11)
	if (Behavior && Behavior->Mode == ETHEnemyBehaviorMode::Stalker && Behavior->bCrouches)
	{
		CrouchDown();
		return;
	}
	if (Body && Body->bIsCrouched)
	{
		Body->UnCrouch();
	}
}

bool ATHEnemyAIController::HasArrived() const
{
	if (LastMoveRequestTime < 0.0 || LastMoveRequest == EPathFollowingRequestResult::Failed)
	{
		return true;
	}
	return GetMoveStatus() == EPathFollowingStatus::Idle;
}

bool ATHEnemyAIController::IsStuck()
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	const bool bTrying = Body && GetMoveStatus() == EPathFollowingStatus::Moving && Now() - LastMoveRequestTime > 0.5;
	if (!bTrying || Body->GetVelocity().Size2D() > 25.0f)
	{
		SlowSince = -1.0;
		return false;
	}
	if (SlowSince < 0.0)
	{
		SlowSince = Now();
	}
	return Now() - SlowSince > 1.0;
}

void ATHEnemyAIController::AvoidCurrentGoal()
{
	AvoidSpot = MoveGoal;
	AvoidUntil = Now() + 10.0;
	SlowSince = -1.0;
}

bool ATHEnemyAIController::IsAvoided(const FVector& Location) const
{
	return Now() < AvoidUntil && FVector::Dist2D(Location, AvoidSpot) < 200.0;
}

// Seeing and being seen

bool ATHEnemyAIController::IsBlocked(const FVector& From, const FVector& To) const
{
	// Walls, doors, crates and props: not bodies (other enemies and players don't hide anything)
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(THEnemyCover), false);
	return GetWorld()->LineTraceTestByObjectType(From, To, Objects, Params);
}

namespace
{
	/** Heights above the feet that must be out of sight: chest and head, standing or crouched. */
	void BodyHeights(bool bCrouched, float& OutLow, float& OutHigh)
	{
		OutLow = bCrouched ? 60.0f : 120.0f;
		OutHigh = bCrouched ? 105.0f : 165.0f;	// head: about 159 standing, 102 crouched (Functional.EnemyBehaviors)
	}
}

bool ATHEnemyAIController::IsAnyPartVisible(const FVector& From, const FVector& FeetLocation, bool bCrouched, bool bWholeBody) const
{
	// The middle of its chest and head; for the whole body also the top of its head and its shoulders either side (seen
	// from From). Checking the middle only, a stalker behind a crate's edge with a shoulder showing, or its head just
	// over the top, counted as hidden and sat there while the player looked straight at it (developer, 2026-11-12)
	float Low, High;
	BodyHeights(bCrouched, Low, High);
	if (!IsBlocked(From, FeetLocation + FVector(0.0f, 0.0f, Low)) || !IsBlocked(From, FeetLocation + FVector(0.0f, 0.0f, High)))
	{
		return true;
	}
	if (!bWholeBody)
	{
		return false;
	}
	const ATHEnemyCharacter* Body = Enemy.Get();
	const float Width = Body ? Body->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.7f : 24.0f;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, (FeetLocation - From).GetSafeNormal2D()) * Width;
	const FVector Middle = FeetLocation + FVector(0.0f, 0.0f, (Low + High) * 0.5f);
	return !IsBlocked(From, FeetLocation + FVector(0.0f, 0.0f, High + 6.0f)) || !IsBlocked(From, Middle + Side) || !IsBlocked(From, Middle - Side);
}

bool ATHEnemyAIController::CanBeSeenFrom(const APawn* Player, const FVector& FeetLocation, bool bCrouched, bool bWholeBody) const
{
	return IsAnyPartVisible(Player->GetPawnViewLocation(), FeetLocation, bCrouched, bWholeBody);
}

bool ATHEnemyAIController::IsCoveredFrom(const APawn* Player, const FVector& FeetLocation, bool bCrouched) const
{
	// Out of sight from where they are and from a step either side or towards it: real cover, not the edge of something
	const FVector Eyes = Player->GetPawnViewLocation();
	const FVector Toward = (FeetLocation - Eyes).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Toward);
	for (const FVector& From : { Eyes, Eyes + Side * 120.0f, Eyes - Side * 120.0f, Eyes + Toward * 100.0f })
	{
		if (IsAnyPartVisible(From, FeetLocation, bCrouched, true))
		{
			return false;
		}
	}
	return true;
}

bool ATHEnemyAIController::FindCoverNearby(FVector& OutLocation) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ATHEnemyCharacter* Body = Enemy.Get();
	if (!Navigation || !Body)
	{
		return false;
	}
	const bool bCrouched = CoversCrouched();
	const FVector Feet = GetFeetLocation();
	TArray<const APawn*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Player && IsTargetable(Player))
		{
			Players.Add(Player);
		}
	}
	// A few steps round it, nearest first: out of every player's sight, all of it, and a short walk (round the crate,
	// not across the open)
	for (const float Radius : { 60.0f, 120.0f, 180.0f, 250.0f })
	{
		double Best = TNumericLimits<double>::Max();
		bool bFound = false;
		for (int32 Step = 0; Step < 12; ++Step)
		{
			FNavLocation Projected;
			const FVector Candidate = Feet + FVector(Radius, 0.0f, 0.0f).RotateAngleAxis(Step * 30.0f, FVector::UpVector);
			if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(40.0f, 40.0f, 200.0f)) || IsAvoided(Projected.Location))
			{
				continue;
			}
			bool bOutOfSight = true;
			for (const APawn* Player : Players)
			{
				if (CanBeSeenFrom(Player, Projected.Location, bCrouched, true))
				{
					bOutOfSight = false;
					break;
				}
			}
			if (!bOutOfSight)
			{
				continue;
			}
			const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Projected.Location, GetPawn());
			if (!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength() > Radius * 2.5f + 100.0f)
			{
				continue;
			}
			if (Path->GetPathLength() < Best)
			{
				Best = Path->GetPathLength();
				OutLocation = Projected.Location;
				bFound = true;
			}
		}
		if (bFound)
		{
			return true;
		}
	}
	return false;
}

bool ATHEnemyAIController::IsInViewOf(const APawn* Player, const FVector& FeetLocation, bool bCrouched, bool bWholeBody) const
{
	if (!Player || !Behavior)
	{
		return false;
	}
	const FVector Eyes = Player->GetPawnViewLocation();
	if (FVector::Dist(Eyes, FeetLocation) > Behavior->LoseSightRadius)
	{
		return false;
	}
	// Sideways only: where they face, not where they look up or down
	if (!IsFacingTowards(Player, FeetLocation, Behavior->ViewAngle))
	{
		return false;
	}
	return CanBeSeenFrom(Player, FeetLocation, bCrouched, bWholeBody);
}

bool ATHEnemyAIController::IsInAnyonesView(const FVector& FeetLocation, bool bCrouched, bool bWholeBody) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Player && IsTargetable(Player) && IsInViewOf(Player, FeetLocation, bCrouched, bWholeBody))
		{
			return true;
		}
	}
	return false;
}

bool ATHEnemyAIController::IsFacingTowards(const APawn* Player, const FVector& Location, float Angle) const
{
	const FVector Facing = Player->GetViewRotation().Vector().GetSafeNormal2D();
	const FVector ToLocation = (Location - Player->GetActorLocation()).GetSafeNormal2D();
	if (Facing.IsNearlyZero() || ToLocation.IsNearlyZero())
	{
		return true;
	}
	return FVector::DotProduct(Facing, ToLocation) >= FMath::Cos(FMath::DegreesToRadians(Angle));
}

FVector ATHEnemyAIController::GetFeetLocation() const
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	return Body->GetActorLocation() - FVector(0.0f, 0.0f, Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

bool ATHEnemyAIController::FindHidingSpot(const APawn* Player, FVector& OutLocation, bool& bOutCrouch, bool& bOutCovered) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ATHEnemyCharacter* Body = Enemy.Get();
	if (!Navigation || !Player || !Body)
	{
		return false;
	}
	const bool bCrouched = CoversCrouched();
	const FVector Self = Body->GetActorLocation();
	const FVector Feet = GetFeetLocation();
	const FVector PlayerLocation = Player->GetActorLocation();
	const double Current = FVector::Dist2D(Self, PlayerLocation);
	// A real move (no hopping a couple of metres): at least HideDistance from them, and not much closer than it is
	const double Keep = FMath::Max(static_cast<double>(Behavior->HideDistance), Current - 300.0);

	// Places around it, in cover (standing, or crouched behind something low) or not; ranked roughly first by how good
	// an escape they look (near it, far from them, cover), so doorways into other rooms aren't crowded out by corners
	struct FCandidate { FVector Spot; bool bCrouch; bool bCovered; double Rough; };
	TArray<FCandidate> Candidates;
	for (const float Radius : { 300.0f, 600.0f, 900.0f, 1200.0f, 1600.0f })
	{
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const FVector Candidate = Feet + FVector(Radius, 0.0f, 0.0f).RotateAngleAxis(Step * 22.5f, FVector::UpVector);
			FNavLocation Projected;
			if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(120.0f, 120.0f, 300.0f)))
			{
				continue;
			}
			const FVector Spot = Projected.Location;
			const double FromThem = FVector::Dist2D(Spot, PlayerLocation);
			if (FromThem < Keep || IsAvoided(Spot))
			{
				continue;
			}
			const bool bStanding = IsCoveredFrom(Player, Spot, false);
			const bool bLow = !bStanding && CoversCrouched() && IsCoveredFrom(Player, Spot, true);
			const bool bCovered = bStanding || bLow;
			Candidates.Add({ Spot, bLow, bCovered, FVector::Dist2D(Feet, Spot) - 0.8 * FromThem + (bCovered ? 0.0 : 1500.0) });
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Rough < B.Rough; });

	// Of the best sixteen, the best escape by walking (developer: it ran into a corner with no way out): short to
	// reach, far from them by walking distance (through a doorway, into another room), not a corner or dead end, in
	// cover if at all possible; never past them on the way
	double Best = TNumericLimits<double>::Max();
	bool bFound = false;
	const int32 Count = FMath::Min(Candidates.Num(), 16);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCandidate& Candidate = Candidates[Index];
		const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Self, Candidate.Spot, GetPawn());
		if (!Path || !Path->IsValid() || Path->IsPartial())
		{
			continue;
		}
		bool bPassesThem = false;
		for (int32 Point = 1; Point < Path->PathPoints.Num(); ++Point)
		{
			if (FMath::PointDistToSegment(PlayerLocation, Path->PathPoints[Point - 1], Path->PathPoints[Point]) < 250.0f)
			{
				bPassesThem = true;
				break;
			}
		}
		if (bPassesThem)
		{
			continue;
		}
		const UNavigationPath* TheirPath = Navigation->FindPathToLocationSynchronously(GetWorld(), PlayerLocation, Candidate.Spot, GetPawn());
		const double TheirWalk = TheirPath && TheirPath->IsValid() ? TheirPath->GetPathLength() : FVector::Dist2D(PlayerLocation, Candidate.Spot);
		// Room to keep going from there: of eight directions, how many are open floor for 3 m (a corner has two or three)
		int32 Open = 0;
		for (int32 Step = 0; Step < 8; ++Step)
		{
			FVector Hit;
			const FVector Out = Candidate.Spot + FVector(300.0f, 0.0f, 0.0f).RotateAngleAxis(Step * 45.0f, FVector::UpVector);
			Open += UNavigationSystemV1::NavigationRaycast(GetWorld(), Candidate.Spot, Out, Hit) ? 0 : 1;
		}
		const double Cost = Path->GetPathLength() - 0.8 * TheirWalk + (8 - Open) * 150.0
			+ (Candidate.bCovered ? 0.0 : 1500.0) + (Candidate.bCrouch && !bCrouched ? 200.0 : 0.0);
		if (Cost < Best)
		{
			Best = Cost;
			OutLocation = Candidate.Spot;
			bOutCrouch = Candidate.bCrouch;
			bOutCovered = Candidate.bCovered;
			bFound = true;
		}
	}
	return bFound;
}

bool ATHEnemyAIController::FindStalkSpot(const APawn* Player, FVector& OutLocation) const
{
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation || !Player)
	{
		return false;
	}
	// All the way round them, nearer and further than StalkDistance: any place out of their view (well outside where they
	// face, or behind a wall), preferring behind them (behind the way they are walking, when they walk steadily), where it
	// can see them (in the same room, behind their back: developer, 2026-11-13, it watched from the next room through a
	// wall and never came back), and a short walk from where it is
	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector Behind = -GetQuarryForward(Player);
	const FVector Self = GetPawn()->GetActorLocation();
	const bool bCrouched = CoversCrouched();
	struct FCandidate { FVector Spot; double OffBehind; double Scaled; double Rough; };
	TArray<FCandidate> Candidates;
	for (const float Scale : { 1.0f, 0.75f, 1.3f, 0.6f })
	{
		// Never inside the distance at which it would feel approached
		const float Radius = FMath::Max(Behavior->StalkDistance * Scale, Behavior->RetreatDistance * 1.3f);
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const float Angle = Step * 22.5f;
			const FVector Candidate = PlayerLocation + Behind.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
			FNavLocation Projected;
			if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(200.0f, 200.0f, 300.0f))
				|| IsAvoided(Projected.Location))
			{
				continue;
			}
			const bool bLine = CanBeSeenFrom(Player, Projected.Location);
			if (bLine && IsFacingTowards(Player, Projected.Location, Behavior->ViewAngle + 45.0f))
			{
				continue;
			}
			const double OffBehind = Angle <= 180.0f ? Angle : 360.0f - Angle;
			const double Scaled = FMath::Abs(Scale - 1.0f) * 300.0 + (bLine ? 0.0 : 800.0);
			Candidates.Add({ Projected.Location, OffBehind, Scaled, OffBehind * 10.0 + FVector::Dist2D(Self, Projected.Location) + Scaled });
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Rough < B.Rough; });

	// Of the best eight, by the walk there (developer, 2026-11-13: moving round behind walls): its length, how much of it
	// a player could see it on (ExposureCost metres per metre: the long way round, through the next room, beats a dash
	// across their view), and never close past them
	double Best = TNumericLimits<double>::Max();
	bool bFound = false;
	const int32 Count = FMath::Min(Candidates.Num(), 8);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCandidate& Candidate = Candidates[Index];
		const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Self, Candidate.Spot, GetPawn());
		if (!Path || !Path->IsValid() || Path->IsPartial())
		{
			continue;
		}
		const double Cost = Candidate.OffBehind * 10.0 + Path->GetPathLength() + Candidate.Scaled
			+ ExposedLength(Path, bCrouched) * Behavior->ExposureCost
			+ (PassesNear(Path, PlayerLocation, Behavior->RetreatDistance * 0.8f) ? 2000.0 : 0.0);
		if (Cost < Best)
		{
			Best = Cost;
			OutLocation = Candidate.Spot;
			bFound = true;
		}
	}
	return bFound;
}

bool ATHEnemyAIController::FindReachSpot(const APawn* Player, FVector& OutLocation) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ATHEnemyCharacter* Body = Enemy.Get();
	if (!Navigation || !Player || !Body)
	{
		return false;
	}
	// Round them, just inside its reach: a place with nothing between it and them, the shortest walk away
	const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius() + Player->GetSimpleCollisionRadius() + Behavior->AttackRange * 0.6f;
	const FVector PlayerLocation = Player->GetActorLocation();
	const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	double Best = TNumericLimits<double>::Max();
	bool bFound = false;
	for (int32 Step = 0; Step < 12; ++Step)
	{
		FNavLocation Projected;
		const FVector Candidate = PlayerLocation + FVector(Radius, 0.0f, 0.0f).RotateAngleAxis(Step * 30.0f, FVector::UpVector);
		if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(40.0f, 40.0f, 200.0f))
			|| FVector::Dist2D(Projected.Location, PlayerLocation) > Radius + 30.0f)
		{
			continue;
		}
		const FVector Middle = Projected.Location + FVector(0.0f, 0.0f, HalfHeight);
		if (!Body->HasClearReach(Player, &Middle))
		{
			continue;
		}
		const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Projected.Location, GetPawn());
		if (!Path || !Path->IsValid() || Path->IsPartial())
		{
			continue;
		}
		if (Path->GetPathLength() < Best)
		{
			Best = Path->GetPathLength();
			OutLocation = Projected.Location;
			bFound = true;
		}
	}
	return bFound;
}

bool ATHEnemyAIController::IsWatchedAt(const FVector& FeetLocation, bool bCrouched) const
{
	// In view with a glance: a clear line, within their view angle and 45 degrees more (behind their back it creeps)
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Player && IsTargetable(Player) && FVector::Dist(Player->GetPawnViewLocation(), FeetLocation) <= Behavior->LoseSightRadius
			&& IsFacingTowards(Player, FeetLocation, Behavior->ViewAngle + 45.0f) && CanBeSeenFrom(Player, FeetLocation, bCrouched, false))
		{
			return true;
		}
	}
	return false;
}

bool ATHEnemyAIController::IsExposedAt(const FVector& FeetLocation, bool bCrouched) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Player && IsTargetable(Player) && FVector::Dist(Player->GetPawnViewLocation(), FeetLocation) <= Behavior->LoseSightRadius
			&& CanBeSeenFrom(Player, FeetLocation, bCrouched, false))
		{
			return true;
		}
	}
	return false;
}

float ATHEnemyAIController::ExposedLength(const UNavigationPath* Path, bool bCrouched) const
{
	float Exposed = 0.0f;
	if (!Path)
	{
		return Exposed;
	}
	for (int32 Point = 1; Point < Path->PathPoints.Num(); ++Point)
	{
		const FVector& From = Path->PathPoints[Point - 1];
		const FVector& To = Path->PathPoints[Point];
		const float Length = FVector::Dist(From, To);
		const int32 Samples = FMath::Max(1, FMath::CeilToInt(Length / 150.0f));
		for (int32 Sample = 1; Sample <= Samples; ++Sample)
		{
			if (IsWatchedAt(FMath::Lerp(From, To, static_cast<float>(Sample) / Samples), bCrouched))
			{
				Exposed += Length / Samples;
			}
		}
	}
	return Exposed;
}

bool ATHEnemyAIController::PassesNear(const UNavigationPath* Path, const FVector& Location, float Distance)
{
	if (!Path)
	{
		return false;
	}
	for (int32 Point = 1; Point < Path->PathPoints.Num(); ++Point)
	{
		if (FMath::PointDistToSegment(Location, Path->PathPoints[Point - 1], Path->PathPoints[Point]) < Distance)
		{
			return true;
		}
	}
	return false;
}

FVector ATHEnemyAIController::GetQuarryForward(const APawn* Player) const
{
	// Walking steadily for a second: the way they go (steadier than their view, which flicks about); else where they face
	if (Player == Quarry.Get() && QuarryMovingSince >= 0.0 && Now() - QuarryMovingSince > 1.0 && !QuarryHeading.IsNearlyZero())
	{
		return QuarryHeading;
	}
	const FVector Facing = Player->GetViewRotation().Vector().GetSafeNormal2D();
	return Facing.IsNearlyZero() ? FVector::ForwardVector : Facing;
}

double ATHEnemyAIController::DistanceFromTeam(const APawn* Player) const
{
	double Nearest = -1.0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Other = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Other || Other == Player || !IsTargetable(Other) || !HealthOf(Other)->IsAlive())
		{
			continue;
		}
		const double Away = FVector::Dist2D(Other->GetActorLocation(), Player->GetActorLocation());
		Nearest = Nearest < 0.0 ? Away : FMath::Min(Nearest, Away);
	}
	return FMath::Max(Nearest, 0.0);
}

bool ATHEnemyAIController::HasOpening(const APawn* Player) const
{
	if (!Player)
	{
		return false;
	}
	// Busy with their gun: reloading, or shooting (at something else) in the last two seconds
	const ATHCharacter* PlayerCharacter = Cast<ATHCharacter>(Player);
	const UTHWeaponComponent* Weapons = PlayerCharacter ? PlayerCharacter->GetWeaponComponent() : nullptr;
	const ATHWeaponBase* Weapon = Weapons ? Weapons->GetCurrentWeapon() : nullptr;
	if (Weapon && (Weapon->IsReloading() || Now() - Weapon->GetLastServerShotTime() < 2.0))
	{
		return true;
	}
	// Standing still a while, or cut off from the others
	if (Player == Quarry.Get() && QuarryStillSince >= 0.0 && Now() - QuarryStillSince > 2.0)
	{
		return true;
	}
	return DistanceFromTeam(Player) > Behavior->StragglerDistance;
}

APawn* ATHEnemyAIController::ChoosePrey(APawn* Current) const
{
	// The straggler (far from the others, up to 30 m counted), then the most hurt; nearer is a little better, and it
	// sticks with the one it has unless another is clearly better (no flitting between them)
	const APawn* Self = GetPawn();
	const float Range = FMath::Max(Behavior->SenseDistance, Behavior->LoseSightRadius);
	auto Score = [this, Self](const APawn* Player)
	{
		const UTHHealthComponent* Health = HealthOf(Player);
		const double Hurt = Health && Health->GetMaxHealth() > 0.0f ? 1.0 - Health->GetHealth() / Health->GetMaxHealth() : 0.0;
		return FMath::Min(DistanceFromTeam(Player), 3000.0) + Hurt * 1500.0 - FVector::Dist2D(Self->GetActorLocation(), Player->GetActorLocation()) * 0.3;
	};
	APawn* Best = Current;
	double BestScore = Current ? Score(Current) + 600.0 : -1.0e18;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Player = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Player || Player == Current || !IsTargetable(Player) || !HealthOf(Player)->IsAlive()
			|| FVector::Dist2D(Player->GetActorLocation(), Self->GetActorLocation()) > Range)
		{
			continue;
		}
		const double PlayerScore = Score(Player);
		if (PlayerScore > BestScore)
		{
			BestScore = PlayerScore;
			Best = Player;
		}
	}
	return Best;
}

bool ATHEnemyAIController::FindWaylaySpot(const APawn* Player, FVector& OutLocation) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const APawn* Self = GetPawn();
	if (!Navigation || !Player || !Self)
	{
		return false;
	}
	// Where they will be: WaylayAhead along their way (on the navigation mesh); its spot a little off that way, beside
	// it, out of their sight now, reached unseen and without passing them, and before they get there (it is faster)
	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector Forward = GetQuarryForward(Player);
	FNavLocation Ahead;
	if (!Navigation->ProjectPointToNavigation(PlayerLocation + Forward * Behavior->WaylayAhead, Ahead, FVector(300.0f, 300.0f, 300.0f)))
	{
		return false;
	}
	const bool bCrouched = CoversCrouched();
	double Best = TNumericLimits<double>::Max();
	bool bFound = false;
	int32 Tried = 0;
	for (const float Radius : { 250.0f, 400.0f })
	{
		for (int32 Step = 0; Step < 8 && Tried < 10; ++Step)
		{
			const FVector Candidate = Ahead.Location + Forward.RotateAngleAxis(Step * 45.0f, FVector::UpVector) * Radius;
			FNavLocation Projected;
			if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(80.0f, 80.0f, 300.0f)) || IsAvoided(Projected.Location)
				|| FVector::DotProduct(Projected.Location - PlayerLocation, Forward) < Behavior->WaylayAhead * 0.5f
				|| IsExposedAt(Projected.Location, bCrouched))
			{
				continue;
			}
			++Tried;
			const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Self->GetActorLocation(), Projected.Location, GetPawn());
			if (!Path || !Path->IsValid() || Path->IsPartial() || PassesNear(Path, PlayerLocation, Behavior->RetreatDistance * 0.8f))
			{
				continue;
			}
			// It must get there well before them: its run against their walk (about a third of its speed)
			const double TheirWalk = FVector::Dist2D(PlayerLocation, Projected.Location);
			if (Path->GetPathLength() > TheirWalk * 2.5)
			{
				continue;
			}
			const double Exposed = ExposedLength(Path, bCrouched);
			if (Exposed > 300.0)
			{
				continue;
			}
			const double Cost = Path->GetPathLength() + Exposed * Behavior->ExposureCost;
			if (Cost < Best)
			{
				Best = Cost;
				OutLocation = Projected.Location;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool ATHEnemyAIController::FindShowSpots(const APawn* Player, FVector& OutShow, FVector& OutCover) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const APawn* Self = GetPawn();
	if (!Navigation || !Player || !Self)
	{
		return false;
	}
	// A place in their view (well inside it), ShowDistance off; cover a step or three beside it that nobody can see it
	// in; and a way to that cover that keeps out of sight
	const bool bCrouched = CoversCrouched();
	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector Facing = Player->GetViewRotation().Vector().GetSafeNormal2D();
	double Best = TNumericLimits<double>::Max();
	bool bFound = false;
	int32 Tried = 0;
	for (int32 Index = 0; Index < 12 && Tried < 4; ++Index)
	{
		const float Angle = FMath::FRandRange(-Behavior->ViewAngle * 0.5f, Behavior->ViewAngle * 0.5f);
		const float Range = FMath::FRandRange(Behavior->ShowDistance.X, Behavior->ShowDistance.Y);
		FNavLocation Show;
		if (!Navigation->ProjectPointToNavigation(PlayerLocation + Facing.RotateAngleAxis(Angle, FVector::UpVector) * Range, Show, FVector(150.0f, 150.0f, 300.0f))
			|| FVector::Dist2D(Show.Location, PlayerLocation) < Behavior->ShowDistance.X * 0.8f
			|| !IsInViewOf(Player, Show.Location, bCrouched, false))
		{
			continue;
		}
		for (int32 Step = 0; Step < 8; ++Step)
		{
			FNavLocation Cover;
			const FVector Beside = Show.Location + FVector(220.0f, 0.0f, 0.0f).RotateAngleAxis(Step * 45.0f, FVector::UpVector);
			if (!Navigation->ProjectPointToNavigation(Beside, Cover, FVector(60.0f, 60.0f, 300.0f)) || IsAvoided(Cover.Location)
				|| IsExposedAt(Cover.Location, bCrouched))
			{
				continue;
			}
			const UNavigationPath* Out = Navigation->FindPathToLocationSynchronously(GetWorld(), Cover.Location, Show.Location, GetPawn());
			if (!Out || !Out->IsValid() || Out->IsPartial() || Out->GetPathLength() > 450.0f)
			{
				continue;
			}
			++Tried;
			const UNavigationPath* There = Navigation->FindPathToLocationSynchronously(GetWorld(), Self->GetActorLocation(), Cover.Location, GetPawn());
			if (!There || !There->IsValid() || There->IsPartial() || PassesNear(There, PlayerLocation, Behavior->RetreatDistance))
			{
				break;
			}
			const double Exposed = ExposedLength(There, bCrouched);
			const double Cost = There->GetPathLength() + Exposed * Behavior->ExposureCost;
			if (Exposed < 300.0 && Cost < Best)
			{
				Best = Cost;
				OutShow = Show.Location;
				OutCover = Cover.Location;
				bFound = true;
			}
			break;
		}
	}
	return bFound;
}

bool ATHEnemyAIController::FindPlaceAwayFrom(const APawn* Player, const FVector& From, const FVector& Direction, float Distance, FVector& OutLocation) const
{
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		return false;
	}
	bool bFound = false;
	double BestDistance = -1.0;
	for (const float Angle : { 0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f })
	{
		const FVector Candidate = From + Direction.RotateAngleAxis(Angle, FVector::UpVector) * Distance;
		FNavLocation Projected;
		if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(300.0f, 300.0f, 300.0f)))
		{
			continue;
		}
		if (!CanBeSeenFrom(Player, Projected.Location))
		{
			OutLocation = Projected.Location;
			return true;
		}
		const double FromPlayer = FVector::Dist2D(Projected.Location, Player->GetActorLocation());
		if (FromPlayer > BestDistance)
		{
			BestDistance = FromPlayer;
			OutLocation = Projected.Location;
			bFound = true;
		}
	}
	return bFound;
}

float ATHEnemyAIController::GapTo(const AActor* Other) const
{
	const ATHEnemyCharacter* Body = Enemy.Get();
	return FVector::Dist2D(Body->GetActorLocation(), Other->GetActorLocation()) - Body->GetCapsuleComponent()->GetScaledCapsuleRadius()
		- Other->GetSimpleCollisionRadius();
}

double ATHEnemyAIController::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float ATHEnemyAIController::RandomIn(const FVector2D& Range) const
{
	return FMath::FRandRange(FMath::Min(Range.X, Range.Y), FMath::Max(Range.X, Range.Y));
}

void ATHEnemyAIController::HoldOff(float Seconds)
{
	// A breather after a peak (the director): a stalker strikes and lures no sooner than this
	NextStrikeTime = FMath::Max(NextStrikeTime, Now() + Seconds);
	NextOpportunityTime = FMath::Max(NextOpportunityTime, Now() + Seconds);
	NextLureTime = FMath::Max(NextLureTime, Now() + Seconds);
}

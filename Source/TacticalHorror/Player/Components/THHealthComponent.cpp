#include "Player/Components/THHealthComponent.h"
#include "Core/THGameMode.h"
#include "Core/THPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UTHHealthComponent::UTHHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTHHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTHHealthComponent, Health);
	DOREPLIFETIME(UTHHealthComponent, HealthState);
	DOREPLIFETIME(UTHHealthComponent, BleedOutServerTime);
}

void UTHHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UTHHealthComponent::HandleTakeAnyDamage);
	}
}

float UTHHealthComponent::GetBleedOutTimeRemaining() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (HealthState != ETHHealthState::Downed || !GameState)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, BleedOutServerTime - GameState->GetServerWorldTimeSeconds());
}

void UTHHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	// Damage while downed is ignored for now; the GDD hasn't decided whether enemies can finish off downed players
	if (Damage <= 0.0f || HealthState != ETHHealthState::Alive)
	{
		return;
	}

	// Friendly fire between players is a game rule, so ask the GameMode
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bVictimIsPlayer = OwnerPawn && OwnerPawn->IsPlayerControlled();
	const bool bInstigatorIsPlayer = InstigatedBy && InstigatedBy->IsPlayerController();
	if (bVictimIsPlayer && bInstigatorIsPlayer && InstigatedBy != OwnerPawn->GetController())
	{
		const ATHGameMode* GameMode = GetWorld()->GetAuthGameMode<ATHGameMode>();
		if (GameMode && !GameMode->IsFriendlyFireEnabled())
		{
			return;
		}
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	OnRep_Health(OldHealth);
	OnHealthChanged.Broadcast(this, Health, DamageCauser);

	if (Health <= 0.0f)
	{
		SetHealthState(bCanBeDowned ? ETHHealthState::Downed : ETHHealthState::Dead, InstigatedBy);
	}
}

void UTHHealthComponent::Heal(float Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0.0f || HealthState != ETHHealthState::Alive)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Min(Health + Amount, MaxHealth);
	OnRep_Health(OldHealth);
	OnHealthChanged.Broadcast(this, Health, nullptr);
}

void UTHHealthComponent::Revive(AController* Reviver)
{
	if (!GetOwner()->HasAuthority() || HealthState != ETHHealthState::Downed)
	{
		return;
	}

	const float OldHealth = Health;
	Health = MaxHealth * ReviveHealthFraction;
	OnRep_Health(OldHealth);
	OnHealthChanged.Broadcast(this, Health, nullptr);

	if (ATHPlayerState* ReviverState = Reviver ? Reviver->GetPlayerState<ATHPlayerState>() : nullptr)
	{
		ReviverState->Revives++;
	}

	SetHealthState(ETHHealthState::Alive, Reviver);
}

void UTHHealthComponent::Kill(AController* Killer)
{
	if (!GetOwner()->HasAuthority() || HealthState == ETHHealthState::Dead)
	{
		return;
	}

	const float OldHealth = Health;
	Health = 0.0f;
	OnRep_Health(OldHealth);
	OnHealthChanged.Broadcast(this, Health, nullptr);
	SetHealthState(ETHHealthState::Dead, Killer);
}

void UTHHealthComponent::BleedOut()
{
	if (HealthState == ETHHealthState::Downed)
	{
		SetHealthState(ETHHealthState::Dead, nullptr);
	}
}

void UTHHealthComponent::SetHealthState(ETHHealthState NewState, AController* Instigator)
{
	if (NewState == HealthState)
	{
		return;
	}

	const ETHHealthState OldState = HealthState;
	HealthState = NewState;

	UWorld* World = GetWorld();
	World->GetTimerManager().ClearTimer(BleedOutTimer);
	BleedOutServerTime = 0.0f;

	if (NewState == ETHHealthState::Downed)
	{
		World->GetTimerManager().SetTimer(BleedOutTimer, this, &UTHHealthComponent::BleedOut, BleedOutDuration, false);
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			BleedOutServerTime = GameState->GetServerWorldTimeSeconds() + BleedOutDuration;
		}
	}

	// Stats
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ATHPlayerState* VictimState = OwnerPawn ? OwnerPawn->GetPlayerState<ATHPlayerState>() : nullptr;
	if (VictimState && NewState == ETHHealthState::Downed)
	{
		VictimState->TimesDowned++;
	}
	if (NewState == ETHHealthState::Dead && OldState == ETHHealthState::Alive)
	{
		// Only count a kill when the hit itself was lethal, not when a downed player bleeds out
		ATHPlayerState* KillerState = Instigator ? Instigator->GetPlayerState<ATHPlayerState>() : nullptr;
		if (KillerState && KillerState != VictimState)
		{
			KillerState->Kills++;
		}
	}
	if (VictimState && NewState == ETHHealthState::Dead)
	{
		VictimState->Deaths++;
	}

	OnRep_HealthState(OldState);

	if (NewState != ETHHealthState::Alive && OwnerPawn && OwnerPawn->IsPlayerControlled())
	{
		if (ATHGameMode* GameMode = World->GetAuthGameMode<ATHGameMode>())
		{
			GameMode->NotifyPlayerIncapacitated(OwnerPawn->GetController());
		}
	}
}

void UTHHealthComponent::OnRep_Health(float OldHealth)
{
	// Server broadcasts with the damage causer in HandleTakeAnyDamage; this keeps clients in sync
	if (!GetOwner()->HasAuthority())
	{
		OnHealthChanged.Broadcast(this, Health, nullptr);
	}
}

void UTHHealthComponent::OnRep_HealthState(ETHHealthState OldState)
{
	OnHealthStateChanged.Broadcast(this, HealthState, OldState);
}

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "THHealthComponent.generated.h"

class UDamageType;

UENUM(BlueprintType)
enum class ETHHealthState : uint8
{
	Alive,
	/** Incapacitated; a teammate can revive before the bleed-out timer expires. */
	Downed,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTHHealthChanged, UTHHealthComponent*, HealthComponent, float, NewHealth, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTHHealthStateChanged, UTHHealthComponent*, HealthComponent, ETHHealthState, NewState, ETHHealthState, OldState);

/**
 * Server-authoritative health with an optional downed state.
 * Listens to the owner's OnTakeAnyDamage, so ApplyDamage / ApplyPointDamage just work.
 * Downed rules (duration, damage while downed) are placeholders until the GDD settles them.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTHHealthComponent();

	/** For an owner's constructor: its default health, whether it can be downed and whether it heals by itself (enemies do neither). */
	void SetDefaults(float InMaxHealth, bool bInCanBeDowned, bool bInRegenerates = true);

	/** Seconds since the last damage taken (server; large if never). */
	float GetTimeSinceDamage() const;

	/** Seconds without damage before health starts coming back. */
	float GetRegenDelay() const { return RegenDelay; }

	bool Regenerates() const { return bRegenerates; }

	/** Server only, before any damage: a different full health (an enemy's behaviour can set its own). */
	void SetMaxHealth(float InMaxHealth);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	ETHHealthState GetHealthState() const { return HealthState; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const { return HealthState == ETHHealthState::Alive; }

	/** Seconds left before a downed player dies. Zero when not downed. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetBleedOutTimeRemaining() const;

	/** Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void Heal(float Amount);

	/** Server only. Brings a downed player back up. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void Revive(AController* Reviver);

	/** Server only. Kills outright, skipping the downed state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void Kill(AController* Killer);

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FTHHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FTHHealthStateChanged OnHealthStateChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void SetHealthState(ETHHealthState NewState, AController* Instigator);
	void BleedOut();

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void OnRep_HealthState(ETHHealthState OldState);

	/** Another player is up (alive) who could revive this one. */
	bool HasTeammateWhoCanRevive() const;

	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = "Health", meta = (ClampMin = 1))
	float MaxHealth = 100.0f;

	/** Players get downed; enemies and props should turn this off and die immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health|Downed")
	bool bCanBeDowned = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health|Downed", meta = (EditCondition = "bCanBeDowned", ClampMin = 1))
	float BleedOutDuration = 30.0f;

	/** Fraction of MaxHealth restored on revive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health|Downed", meta = (EditCondition = "bCanBeDowned", ClampMin = 0.01, ClampMax = 1))
	float ReviveHealthFraction = 0.3f;

	/**
	 * Heals by itself after RegenDelay seconds without damage, RegenRate a second, up to full (developer, 2026-11-12: no
	 * healing items; the blood on screen clears and health comes back slowly). Players only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health|Regeneration")
	bool bRegenerates = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health|Regeneration", meta = (EditCondition = "bRegenerates", ClampMin = 0, Units = "s"))
	float RegenDelay = 5.0f;

	/** Health a second (100 = full health): 5 takes about 20 s from nearly dead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health|Regeneration", meta = (EditCondition = "bRegenerates", ClampMin = 0.1))
	float RegenRate = 5.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Health")
	float Health = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_HealthState, BlueprintReadOnly, Category = "Health")
	ETHHealthState HealthState = ETHHealthState::Alive;

	/** Server time at which a downed player bleeds out. Replicated so clients can show a countdown. */
	UPROPERTY(Replicated)
	float BleedOutServerTime = 0.0f;

	FTimerHandle BleedOutTimer;

	/** Server: waits RegenDelay after a hit, then heals in small steps until full. */
	void StartRegenTimer();
	void RegenStep();
	FTimerHandle RegenTimer;
	double LastDamageTime = -1.0e9;
};

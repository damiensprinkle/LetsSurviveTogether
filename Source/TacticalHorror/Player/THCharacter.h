#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/Components/THHealthComponent.h"
#include "THCharacter.generated.h"

class UInputAction;
class USoundBase;
class UTHDoorSyncComponent;
class UTHPressableComponent;
class UTHWeaponComponent;

/**
 * C++ base for the player character.
 *
 * Deliberately adds no camera, mesh or movement input: the horror template's BP_FirstPersonCharacter keeps those
 * (plus interaction, flashlight, night vision, footsteps). Reparent that Blueprint to this class to add health,
 * weapons, the downed state and reviving.
 */
UCLASS()
class TACTICALHORROR_API ATHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATHCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Character")
	UTHHealthComponent* GetHealthComponent() const { return HealthComponent; }
	class UTHSanityComponent* GetSanityComponent() const { return SanityComponent; }

	UFUNCTION(BlueprintPure, Category = "Character")
	UTHWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** Combined speed multiplier from the held weapon and the downed state. Used by UTHCharacterMovementComponent. */
	float GetMovementSpeedMultiplier() const;

	/** Owning client: progress of the revive currently being performed by this player, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Revive")
	float GetReviveProgress() const;

	/** The downed teammate this player is looking at and close enough to revive, if any. */
	UFUNCTION(BlueprintPure, Category = "Revive")
	ATHCharacter* FindReviveTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Revive")
	void StartRevive();

	UFUNCTION(BlueprintCallable, Category = "Revive")
	void StopRevive();

	/** A template door was pressed on this client (see UTHDoorSyncComponent). Lives here because only an actor the client owns can call the server. */
	UFUNCTION(Server, Reliable)
	void ServerRequestDoor(UTHDoorSyncComponent* Door, bool bOpen);

	/** This client's player entered a keypad door's code. */
	UFUNCTION(Server, Reliable)
	void ServerRequestDoorUnlock(UTHDoorSyncComponent* Door);

	/** The hold-to-turn key for valve doors (F by default). */
	const UInputAction* GetValveAction() const { return ValveAction; }

	// Voice and body sounds, played on every machine (one picked at random; empty = silent)

	/** Hurt (any drop in health), at most every HurtSoundCooldown. */
	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	TArray<TSoftObjectPtr<USoundBase>> HurtSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Sounds", meta = (Units = "s"))
	float HurtSoundCooldown = 0.5f;

	/** Leaving the ground upwards (a jump). */
	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	TArray<TSoftObjectPtr<USoundBase>> JumpSounds;

	/** A death stinger, not placed in the world: full volume for the one who died, OtherPlayerDeathVolume for the others. */
	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	TArray<TSoftObjectPtr<USoundBase>> DeathSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Sounds", meta = (ClampMin = 0))
	float OtherPlayerDeathVolume = 0.6f;

	/** How far the hurt and jump voices carry. */
	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	TSoftObjectPtr<class USoundAttenuation> VoiceAttenuation;

	/**
	 * Dead, the body plays one of these (picked by the server, the same everywhere) and stays as it ends: Darkania Works'
	 * Dramatic Death performances (developer, 2026-11-11: comic relief). Empty = it falls as a ragdoll.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	TArray<TSoftObjectPtr<class UAnimSequence>> DeathPerformances;

	/** The death performance playing on this machine, if any (for tests). */
	class UAnimSequence* GetDeathPerformance() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The last voice or body sound played for this character on this machine (for tests). */
	USoundBase* GetLastSound() const { return LastSound.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	/** ACharacter keeps its own Mesh on the ground when the capsule shrinks; the template's body is a separate component, so do the same for it. */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION()
	void HandleHealthStateChanged(UTHHealthComponent* Health, ETHHealthState NewState, ETHHealthState OldState);

	/** On the owning machine, a drop in health is a hit: tell the player controller (view jolt, blood on screen). */
	UFUNCTION()
	void HandleHealthChanged(UTHHealthComponent* Health, float NewHealth, AActor* DamageCauser);

	/** Dead: the body falls (on every machine, not synced) and the first-person weapon leaves the owner's view. */
	void BecomeRagdoll();

	float LastSeenHealth = -1.0f;
	/** Plays one of Sounds: from the body (attenuated) or, b2D, not placed. */
	void PlayRandomSound(const TArray<TSoftObjectPtr<USoundBase>>& Sounds, bool b2D, float Volume = 1.0f);
	double NextHurtSoundTime = 0.0;
	bool bWasOnGround = true;
	TWeakObjectPtr<USoundBase> LastSound;
	bool bRagdoll = false;

	/** Which of DeathPerformances, picked by the server as they die; 255 = none yet. */
	UPROPERTY(ReplicatedUsing = OnRep_DeathPerformance)
	uint8 DeathPerformance = 255;

	UFUNCTION()
	void OnRep_DeathPerformance();

	/** Plays the picked death performance on the body (once it is dead and the pick is known here). */
	void PlayDeathPerformance();
	bool bPlayingDeathPerformance = false;

public:
	bool IsRagdoll() const { return bRagdoll; }

protected:

	/** Blueprint hook for presentation: camera drop, post-process, ragdoll, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "On Health State Changed"))
	void ReceiveHealthStateChanged(ETHHealthState NewState, ETHHealthState OldState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTHHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UTHSanityComponent> SanityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTHWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UTHFootstepComponent> FootstepComponent;

	/** Movement speed while downed (crawling). 0 pins the player in place. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = 0, ClampMax = 1))
	float DownedSpeedMultiplier = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive", meta = (ClampMin = 0, Units = "s"))
	float ReviveDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive", meta = (ClampMin = 50, Units = "cm"))
	float ReviveRange = 200.0f;

	/** Hold to revive. Defaults to E, created at runtime, and doesn't consume the key so the template's Interact still works. */
	UPROPERTY(EditDefaultsOnly, Category = "Revive")
	TObjectPtr<UInputAction> ReviveAction;

	/** Hold to walk at reduced speed with near-silent footsteps (Shift by default). */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	TObjectPtr<UInputAction> WalkQuietlyAction;

	void StartWalkQuietly();
	void StopWalkQuietly();

	/** Hold to turn a valve door's wheel (F by default). */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	TObjectPtr<UInputAction> ValveAction;

	/** The template's interact action. Bound here as well as in the Blueprint, for the actors the server decides. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	TSoftObjectPtr<UInputAction> TemplateInteractAction;

	/** How far the interact press reaches (cm): the template character's InteractionRange. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = 0, Units = "cm"))
	float InteractRange = 220.0f;

	void HandleInteractPressed();
	void HandleInteractReleased();
	void HandleValvePressed();
	void HandleValveReleased();

private:
	bool CanReviveTarget(const ATHCharacter* Target) const;

	/** This machine's player: their lights flicker with a stalker near (its Light Flicker Distance; the closer, the worse),
	 *  each back to its own brightness after. */
	void UpdateLightFlicker(float DeltaSeconds);
	TMap<TWeakObjectPtr<class ULightComponent>, float> FlickerBase;
	float FlickerLevel = 1.0f;
	double NextFlicker = 0.0;
	double NextFlickerSound = 0.0;

protected:
	/** A stalker near: how long the lights stay on between flickers (s, at random; shorter the closer it is, down to half). */
	UPROPERTY(EditDefaultsOnly, Category = "Flicker")
	FVector2D FlickerSteadyTime = FVector2D(0.6, 1.6);

	/** How long each dip or cut lasts (s, at random). (Developer, 2026-11-14: it flickered too quickly; was 0.04-0.18 s each.) */
	UPROPERTY(EditDefaultsOnly, Category = "Flicker")
	FVector2D FlickerDipTime = FVector2D(0.12, 0.35);

	/** Played at the player's light as it cuts out or dips (one at random; at most every Flicker Sound Gap). Only they hear it. */
	UPROPERTY(EditDefaultsOnly, Category = "Flicker")
	TArray<TObjectPtr<USoundBase>> FlickerSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Flicker", meta = (Units = "s"))
	float FlickerSoundGap = 0.8f;

public:
	/** How much a stalker near has its lights flickering now (0 none .. 1 right next to it; tests). */
	float GetLightFlickerStrength() const { return FlickerStrength; }

private:
	float FlickerStrength = 0.0f;

	UFUNCTION(Server, Reliable)
	void ServerPress(UTHPressableComponent* Target);

	UFUNCTION(Server, Reliable)
	void ServerRelease(UTHPressableComponent* Target);

	/** Local: the valve this player is holding. */
	UPROPERTY(Transient)
	TObjectPtr<UTHPressableComponent> HeldValve;

	/** What the interact key last pressed, told when the key is released. */
	UPROPERTY(Transient)
	TObjectPtr<UTHPressableComponent> HeldInteract;

	UFUNCTION(Server, Reliable)
	void ServerStartRevive(ATHCharacter* Target);

	UFUNCTION(Server, Reliable)
	void ServerStopRevive();

	void CompleteRevive();

	TWeakObjectPtr<ATHCharacter> ReviveTarget;
	FTimerHandle ReviveTimer;
	float LocalReviveStartTime = -1.0f;
};

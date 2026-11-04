#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/Components/THHealthComponent.h"
#include "THCharacter.generated.h"

class UInputAction;
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	/** ACharacter keeps its own Mesh on the ground when the capsule shrinks; the template's body is a separate component, so do the same for it. */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION()
	void HandleHealthStateChanged(UTHHealthComponent* Health, ETHHealthState NewState, ETHHealthState OldState);

	/** Blueprint hook for presentation: camera drop, post-process, ragdoll, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "On Health State Changed"))
	void ReceiveHealthStateChanged(ETHHealthState NewState, ETHHealthState OldState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTHHealthComponent> HealthComponent;

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

private:
	bool CanReviveTarget(const ATHCharacter* Target) const;

	UFUNCTION(Server, Reliable)
	void ServerStartRevive(ATHCharacter* Target);

	UFUNCTION(Server, Reliable)
	void ServerStopRevive();

	void CompleteRevive();

	TWeakObjectPtr<ATHCharacter> ReviveTarget;
	FTimerHandle ReviveTimer;
	float LocalReviveStartTime = -1.0f;
};

#pragma once

#include "CoreMinimal.h"
#include "Multiplayer/THPressableComponent.h"
#include "THKeyPickupComponent.generated.h"

class USoundBase;

/**
 * BP_Key in co-op: the server gives the key to the player who pressed it (the template's Has Key? on their pawn) and
 * takes it out of the world for everyone. If that player leaves still holding it, it comes back where it was, so the team
 * can't be locked out. Unlocking a BP_DoorLock uses it up for good. As in the template, Has Key? is one flag, so a key fits
 * any BP_DoorLock, and a player can only carry one.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHKeyPickupComponent : public UTHPressableComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ServerPressed(ATHCharacter* By) override;

	/** Server: whether this player holds a key. */
	static bool PlayerHasKey(const AActor* Player);

	/** Server: this player's key has unlocked a door. They no longer hold it, and it never comes back. */
	static void UseKey(AActor* Player);

	/** Whether a player is holding this key, so it's out of the world. */
	bool IsTaken() const { return bTaken; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Key")
	TObjectPtr<USoundBase> PickupSound;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayPickupSound();

	UFUNCTION()
	void OnRep_Taken();

	/** Server: the holder's pawn is going away. */
	UFUNCTION()
	void HandleHolderEndPlay(AActor* LeavingHolder, EEndPlayReason::Type Reason);

	void SetTaken(bool bNewTaken);

	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;

	/** Server: who is carrying this key. */
	TWeakObjectPtr<AActor> Holder;
};

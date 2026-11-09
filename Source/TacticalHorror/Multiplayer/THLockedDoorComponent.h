#pragma once

#include "CoreMinimal.h"
#include "Multiplayer/THPressableComponent.h"
#include "THLockedDoorComponent.generated.h"

class USoundBase;
class UTimelineComponent;

/**
 * BP_DoorLock in co-op. Only a player holding a key can unlock it (anyone else just rattles it, and everyone hears
 * that). The first opening unlocks it for good: from then on any player can open and close it.
 * The door keeps the Blueprint's timelines, so it moves and sounds as the template made it.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHLockedDoorComponent : public UTHPressableComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ServerPressed(ATHCharacter* By) override;

	bool IsUnlocked() const { return bUnlocked; }
	bool IsOpen() const { return bOpen; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Door")
	TObjectPtr<USoundBase> LockedSound;

	UPROPERTY(EditDefaultsOnly, Category = "Door")
	TObjectPtr<USoundBase> UnlockSound;

	UPROPERTY(EditDefaultsOnly, Category = "Door")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditDefaultsOnly, Category = "Door")
	TObjectPtr<USoundBase> CloseSound;

private:
	UFUNCTION()
	void OnRep_Open();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRattle();

	/** Runs the door's timeline to bOpen on this machine. */
	void ApplyOpenState();
	void PlayDoorSound(USoundBase* Sound) const;

	UPROPERTY(Replicated)
	bool bUnlocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UPROPERTY(Transient)
	TObjectPtr<UTimelineComponent> DoorTimeline;

	UPROPERTY(Transient)
	TObjectPtr<UTimelineComponent> RattleTimeline;

	/** The rattle goes out and back on alternate presses, as the template's does. */
	bool bRattleForward = true;
	bool bPlayedUnlock = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "THDoorSyncComponent.generated.h"

class APawn;
class FBoolProperty;
class UFunction;

/**
 * Makes one of the template's doors work in co-op. The template's doors only run on the machine whose player pressed
 * interact, and the character has no server event for it. This component leaves the door's Blueprint logic alone:
 * the server keeps the replicated open / closed state, and every machine runs the door's own Interact event to reach
 * it, so each plays the same animation and sound.
 *
 * A client's press moves its door at once, then asks the server (ATHCharacter::ServerRequestDoor, because only a pawn
 * the client owns can send to the server). If the server refuses, the client's door goes back.
 *
 * On BP_KeypadDoor it also shares the unlock: when one player enters the code (the widget sets CodeCorrect?), the door
 * is unlocked on every machine, so everyone can then use it. Each player can still enter the code on their own screen.
 *
 * Added to BP_Door, BP_Outward_Door, BP_SlideDoor and BP_KeypadDoor by Scripts/Tools/setup_door_replication.py.
 * BP_DoorLock and BP_ValveDoor work differently: see UTHLockedDoorComponent and UTHValveDoorComponent.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHDoorSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTHDoorSyncComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: a player asks for the door to end up open or closed. Ignored if they are too far away to have pressed it.
	 *  Slammed (a charging enemy): it swings open Slam Play Rate times as fast, on every machine. True if taken and the door
	 *  isn't already that way (it will move). */
	bool HandleRequest(APawn* InRequester, bool bOpen, bool bSlam = false);

	/** Server: a player entered this door's code on their screen. */
	void HandleUnlockRequest(APawn* InRequester);

	bool IsUnlockedOnServer() const { return bServerUnlocked; }

	/** Not locked (anyone, an enemy too, can open it). */
	bool CanBeOpened() const { return !IsLocked(); }

	/** Server: shut and still (an enemy stopped by it bangs on it). */
	bool IsShut() const { return !bServerOpen && !IsDoorMoving(); }

protected:
	/** How far from the door (cm) the server accepts a player's request. Above the template's interaction range, for lag. */
	UPROPERTY(EditDefaultsOnly, Category = "Door")
	float MaxRequestDistance = 500.0f;

	/** How long (seconds) a client waits for the server to agree with its press before following the server again. */
	UPROPERTY(EditDefaultsOnly, Category = "Door")
	float RequestTimeout = 2.0f;

	/** Slammed open by a charging enemy (developer, 2026-11-13: the charger opened doors so slowly it ran into them): the door's
	 *  own swing (its Blueprint timelines) played this many times as fast. */
	UPROPERTY(EditDefaultsOnly, Category = "Door", meta = (ClampMin = 1))
	float SlamPlayRate = 7.0f;

private:
	bool IsDoorOpen() const;
	bool IsDoorMoving() const;
	/** Runs the door's own Interact event, which toggles it. */
	void RunDoorInteract(APawn* Player);
	APawn* GetLocalPawn() const;

	void TickServer(bool bOpen);
	void TickClient(bool bOpen);
	/** Shares CodeCorrect? with the server and from it. Before the door's state: replaying Interact needs it set. */
	void SyncUnlock();
	bool IsLocked() const { return UnlockProperty && !bServerUnlocked; }

	UPROPERTY(Replicated)
	bool bServerOpen = false;

	UPROPERTY(Replicated)
	bool bServerUnlocked = false;

	/** The last time it was opened, it was slammed (so every machine swings it fast). */
	UPROPERTY(Replicated)
	bool bServerSlam = false;
	bool bRequestedSlam = false;
	/** Its timelines are sped up (a slam) and go back to normal once it stops. */
	bool bSpedUp = false;
	void SetSwingRate(float Rate);

	/**
	 * Which way it swings. BP_Outward_Door works it out as it opens, from where the pressing Player stands (its DotProduct:
	 * 85 degrees one way or the other), so a client replaying Interact with its own pawn, or an enemy opening it (no
	 * Player at all), swung it differently on different machines (developer, 2026-11-13). The server sets it from whoever
	 * asked and every machine copies it. Null on doors without one.
	 */
	const FProperty* SwingProperty = nullptr;
	double GetSwing() const;
	void SetSwing(double Value);
	/** Server: the swing a pawn standing here gives (as the door's Blueprint works it out). */
	double SwingFrom(const FVector& Where) const;

	UPROPERTY(Replicated)
	float ServerSwing = 0.0f;

	/** BP_KeypadDoor's CodeCorrect?. Null on doors with no lock. */
	const FBoolProperty* UnlockProperty = nullptr;
	/** Client: the server has been told about this machine's correct code. */
	bool bUnlockSent = false;

	const FBoolProperty* OpenProperty = nullptr;
	/** BP_SlideDoor keeps IsClosed where the swinging doors keep Opening. */
	bool bOpenPropertyInverted = false;
	const FBoolProperty* MovingProperty = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UFunction> InteractFunction = nullptr;

	/** Server: the state a player asked for, applied once the door has stopped moving. */
	bool bHasRequest = false;
	bool bRequestedOpen = false;
	TWeakObjectPtr<APawn> Requester;

	/** Client: what the door last was, to notice this machine's player pressing it. */
	bool bLastSeenOpen = false;
	/** Client: until this time the server has yet to answer a press, so its state is not followed. */
	double PendingUntil = -1.0;
};

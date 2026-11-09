#pragma once

#include "CoreMinimal.h"
#include "Multiplayer/THPressableComponent.h"
#include "THValveDoorComponent.generated.h"

class UAudioComponent;
class USoundBase;
class UTimelineComponent;

/** What the wheel is doing, sent from the server. */
USTRUCT()
struct FTHValveState
{
	GENERATED_BODY()

	/** 1 turning open, -1 turning closed, 0 still. */
	UPROPERTY()
	int8 Direction = 0;

	/** Where the timeline was when this started, so every machine's wheel and door agree. */
	UPROPERTY()
	float Position = 0.0f;
};

/**
 * BP_ValveDoor in co-op: any player standing at the wheel holds the valve key to turn it, which moves the door. Same
 * rules as the template: hold to turn, let go to stop, and at the fully open end holding turns it back. The server runs
 * it, and every machine plays the Blueprint's own timeline from the server's state. The sounds are at the door, not 2D
 * as in the template, where everyone on the map would hear them as if at the wheel.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHValveDoorComponent : public UTHPressableComponent
{
	GENERATED_BODY()

public:
	UTHValveDoorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void ServerPressed(ATHCharacter* By) override;
	virtual void ServerReleased(ATHCharacter* By) override;
	virtual bool UsesInteractKey() const override { return false; }

	/** Whether this player is standing where they can turn the wheel (the Blueprint's Box). */
	bool IsPlayerAtValve(const AActor* Player) const;

	/** This machine's timeline position, in seconds. */
	float GetPosition() const;

	/** The timeline's length, in seconds. */
	float GetLength() const;

	bool IsTurning() const { return State.Direction != 0; }

protected:
	/** Played while turning it back from fully open. */
	UPROPERTY(EditDefaultsOnly, Category = "Valve")
	TObjectPtr<USoundBase> TurnBackSound;

	/** Played while turning it open. */
	UPROPERTY(EditDefaultsOnly, Category = "Valve")
	TObjectPtr<USoundBase> TurnOpenSound;

	/** The door's own groan, also while turning it open. */
	UPROPERTY(EditDefaultsOnly, Category = "Valve")
	TObjectPtr<USoundBase> DoorMoveSound;

private:
	UFUNCTION()
	void OnRep_State();

	/** Plays State on this machine's timeline and sounds. */
	void ApplyState(bool bSetPosition);
	void StopServerTurning();
	void StopSounds();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FTHValveState State;

	UPROPERTY(Transient)
	TObjectPtr<UTimelineComponent> Timeline;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TurnSound;

	/** Server: who is holding the wheel. A second player's hold does nothing until the first lets go. */
	TWeakObjectPtr<ATHCharacter> Holder;
};

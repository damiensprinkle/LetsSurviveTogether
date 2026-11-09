#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "THPressableComponent.generated.h"

class ATHCharacter;

/**
 * Base for template actors whose interaction has to be decided by the server in co-op (BP_DoorLock, BP_Key,
 * BP_ValveDoor). Those Blueprints read input on one machine, from an overlap that any player's pawn can trigger, so
 * each subclass turns that overlap off and takes the press from ATHCharacter instead, reusing the Blueprint's timelines,
 * meshes and sounds. Their graphs are not changed. Set up by Scripts/Tools/setup_door_replication.py.
 */
UCLASS(Abstract, ClassGroup = (TacticalHorror))
class TACTICALHORROR_API UTHPressableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTHPressableComponent();

	/** Server: the player pressed this. Only called for players within reach. */
	virtual void ServerPressed(ATHCharacter* By) {}

	/** Server: the player let go. Not range checked, so a player who walked away can still let go. */
	virtual void ServerReleased(ATHCharacter* By) {}

	/** Whether the interact key looking at this actor presses it (the valve is held with its own key instead). */
	virtual bool UsesInteractKey() const { return true; }

	bool IsWithinReach(const AActor* Player) const;

protected:
	/** Turns off overlap events on the owner's named component, which keeps the Blueprint's own input path closed. */
	void DisableBlueprintOverlap(FName ComponentName) const;

	/** Finds one of the owner's components by its name in the Blueprint. */
	template <typename T>
	T* FindOwnerComponent(FName Name) const
	{
		TInlineComponentArray<T*> Components(GetOwner());
		for (T* Component : Components)
		{
			if (Component->GetFName() == Name)
			{
				return Component;
			}
		}
		return nullptr;
	}

	/** How far from the actor (cm) the server accepts a press. Above the template's interaction range, for lag. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float MaxPressDistance = 500.0f;
};

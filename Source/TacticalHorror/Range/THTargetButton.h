#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Multiplayer/THPressableComponent.h"
#include "THTargetButton.generated.h"

class ATHTargetCarrier;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** The press half of a target button: holding the interact key on it moves its carrier. */
UCLASS(ClassGroup = (TacticalHorror))
class TACTICALHORROR_API UTHTargetButtonComponent : public UTHPressableComponent
{
	GENERATED_BODY()

public:
	virtual void ServerPressed(ATHCharacter* By) override;
	virtual void ServerReleased(ATHCharacter* By) override;

	UPROPERTY(EditInstanceOnly, Category = "Target Button")
	TObjectPtr<ATHTargetCarrier> Carrier;

	/** 1 sends the target downrange, -1 brings it back. */
	UPROPERTY(EditInstanceOnly, Category = "Target Button")
	int32 Direction = 1;
};

/**
 * A button in a firing-range booth: hold the interact key (E) to move the lane's target, let go to stop. The mesh is a
 * placeholder (the template's light switch); swap it on the placed buttons. Placed by setup_target_carriers.py.
 */
UCLASS()
class TACTICALHORROR_API ATHTargetButton : public AActor
{
	GENERATED_BODY()

public:
	ATHTargetButton();

	UTHTargetButtonComponent* GetButton() const { return Button; }

protected:
	/** What the interact trace hits: blocks Visibility only, so it is easy to aim at and bullets pass through it. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> PressArea;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What the button does, e.g. "SEND" or "RETURN". */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTHTargetButtonComponent> Button;
};

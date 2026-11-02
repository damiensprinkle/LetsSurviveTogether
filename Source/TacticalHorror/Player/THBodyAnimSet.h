#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "THBodyAnimSet.generated.h"

class UAnimSequence;
class UBlendSpace;

/**
 * Animations for the full body other players see (third person), for one way of holding things:
 * unarmed, rifle, pistol. Played by UTHBodyAnimInstance.
 *
 * Legs come from the locomotion slots. A weapon set may leave them empty to reuse the unarmed set's legs.
 * The upper body is replaced from UpperBodyRootBone up by UpperBodyPose, so walking and crouching keep
 * the weapon held. Aim offset and actions are additive and only touch the upper body.
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHBodyAnimSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	bool HasLocomotion() const { return Idle != nullptr; }

	// Locomotion (legs). Directional walks are blended by movement direction and sped up / slowed to match speed.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkForward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkBackward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> WalkRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CrouchIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CrouchForward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CrouchBackward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CrouchLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CrouchRight;

	/** Looped while in the air. Empty = keep walking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Falling;

	/** Limits how far walks are sped up or slowed down to match the character's speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	FVector2D PlayRateRange = FVector2D(0.6f, 1.6f);

	// Upper body

	/** Looped and layered over the legs from UpperBodyRootBone up, e.g. a rifle aiming pose. Empty = no layer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upper Body")
	TObjectPtr<UAnimSequence> UpperBodyPose;

	/** This bone and everything below it in the hierarchy belongs to the upper body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upper Body")
	FName UpperBodyRootBone = TEXT("spine_01");

	/** Additive aim offset; only its pitch is used (the body turns with the view). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upper Body")
	TObjectPtr<UBlendSpace> AimOffset;

	/** The aim offset's pitch input when looking straight up (90 degrees): 1 for -1..1 aim offsets, 90 for -90..90 ones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upper Body")
	float AimOffsetPitchAtLookUp = 1.0f;

	// Actions: additive animations on the upper body, triggered by the weapon on every machine

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UAnimSequence> Fire;

	/** Stretched to the weapon's ReloadDuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UAnimSequence> Reload;

	/** Stretched to the weapon's EquipDuration (but never faster than ×2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UAnimSequence> Equip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ClampMin = 0, Units = "s"))
	float ActionBlendTime = 0.15f;
};

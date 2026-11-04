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
 * Legs come from the locomotion slots; any slot left empty uses the unarmed set's animation instead.
 * While legs come from another set (or a slot fell back), the upper body from UpperBodyRootBone up is replaced by
 * UpperBodyPose so the weapon stays held. Aim offset and actions only touch the upper body.
 * The left hand can be pulled onto the weapon with IK (target supplied by the weapon).
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHBodyAnimSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
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

	/** Speed (cm/s) the walks move at when played normally, for matching play rate to the character's speed. 0 = measure it from the animations' root motion (in-place animations then play at normal rate). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 0, Units = "cm/s"))
	float WalkAnimSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 0, Units = "cm/s"))
	float CrouchAnimSpeed = 0.0f;

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

	/**
	 * Turns the upper body (from UpperBodyRootBone) about the vertical, in degrees; + turns it to the character's right.
	 * Aiming animations often stand bladed (torso turned ~30 degrees to the right), which looks like side-stepping when
	 * the legs walk forward. A negative value squares the torso up; HeadBone turns back so the head still looks ahead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upper Body", meta = (Units = "Degrees"))
	float UpperBodyYaw = 0.0f;

	/** Turned back by UpperBodyYaw so the head keeps facing forward. None = the head turns with the torso. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upper Body")
	FName UpperBodyYawHeadBone = TEXT("neck_01");

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

	/**
	 * Share of the Equip animation before the weapon appears in the hand (third person). The draw brings the empty hand
	 * up from the holster; the weapon shows once the hand is in front. 0 = visible from the start.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ClampMin = 0, ClampMax = 1))
	float EquipRevealFraction = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ClampMin = 0, Units = "s"))
	float ActionBlendTime = 0.15f;

	/**
	 * How long the end of the reload hands the hands back to the weapon (hand IK). The reload animation's own end pose
	 * holds a differently placed gun, so a longer hand-back glides the left hand onto the grip instead of showing that pose.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ClampMin = 0, Units = "s"))
	float ReloadBlendOutTime = 1.0f;

	// Hand IK: puts both hands where the weapon holds them (the weapon data's Third Person section places the weapon
	// from the head). Fades out while a full-pose action (reload, equip) plays; the weapon then follows the right hand.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK", meta = (ClampMin = 0, ClampMax = 1))
	float HandIKWeight = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK")
	FName RightUpperArmBone = TEXT("upperarm_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK")
	FName RightLowerArmBone = TEXT("lowerarm_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK")
	FName RightHandBone = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK")
	FName LeftUpperArmBone = TEXT("upperarm_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK")
	FName LeftLowerArmBone = TEXT("lowerarm_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand IK")
	FName LeftHandBone = TEXT("hand_l");

	// Hand pose: the fingers (every bone below the hand bones above) are copied from one frame of this animation
	// while the hands are on the weapon, e.g. so the index finger is on the trigger.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand Pose")
	TObjectPtr<UAnimSequence> HandPose;

	/** Frame of HandPose to use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand Pose", meta = (ClampMin = 0, Units = "s"))
	float HandPoseTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand Pose")
	bool bHandPoseRight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hand Pose")
	bool bHandPoseLeft = true;

	// Lowered carry: while the player isn't aiming and hasn't shot for ReadyHoldTime, the body plays LoweredSet instead
	// (its own idle and walks, e.g. rifle held low) with the weapon in its right hand. Aiming or shooting brings this
	// set back. Each machine works it out from replicated aiming and the shots it sees.

	/** Body set for carrying the weapon lowered. Empty = always this set. Its hand IK and hand pose are not used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lowered")
	TObjectPtr<UTHBodyAnimSet> LoweredSet;

	/**
	 * Used when this set is someone's LoweredSet: pull the left hand onto the weapon's left grip. Off = the left hand
	 * stays where this set's animations put it (for a pose authored to cradle the weapon).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lowered")
	bool bLeftHandOnWeapon = true;

	/** Seconds after the last shot (or letting go of aim) before lowering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lowered", meta = (ClampMin = 0, Units = "s"))
	float ReadyHoldTime = 7.0f;

	/** Seconds to bring the weapon up, and to lower it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lowered", meta = (ClampMin = 0.01, Units = "s"))
	float RaiseTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lowered", meta = (ClampMin = 0.01, Units = "s"))
	float LowerTime = 0.5f;
};

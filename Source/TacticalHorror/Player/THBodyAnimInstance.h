#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "THBodyAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;
class UTHBodyAnimSet;
class USkeletalMesh;
struct FReferenceSkeleton;

/**
 * How a hand holds the weapon, taken from the first-person rig's hand, in aim space relative to the weapon's grip
 * point (cm). The body's hand is turned and placed so its palm matches, and its fingers point the same ways, which
 * works across skeletons with different hand bone axes.
 */
struct FTHHandShape
{
	static constexpr int32 NumFingers = 5; // thumb, index, middle, ring, pinky
	static constexpr int32 MaxJoints = 4;

	bool bValid = false;
	/** Middle of the knuckles (where the fingers start). */
	FVector Palm = FVector::ZeroVector;
	/** X from the wrist to the knuckles, Y across the knuckles toward the index finger. */
	FQuat Frame = FQuat::Identity;
	/** Per finger, the direction from each joint to the next. */
	FVector FingerDirections[NumFingers][MaxJoints];
	int32 NumDirections[NumFingers] = {};

	/** Finger bones below Hand, found by name (thumb / index / middle / ring / pinky, metacarpals skipped), base first. */
	static void FindFingers(const FReferenceSkeleton& Skeleton, FName Hand, TArray<FName> (&OutFingers)[NumFingers]);
	/** Palm frame from the wrist and the index / middle / pinky knuckles. */
	static FQuat MakeFrame(const FVector& Wrist, const FVector& Index, const FVector& Middle, const FVector& Pinky);
	static FVector MakePalm(const FVector& Wrist, const FVector& Index, const FVector& Middle, const FVector& Pinky);
};

/** Everything the worker thread needs to build one frame's pose. Filled on the game thread. */
struct FTHBodyPoseSnapshot
{
	struct FSample
	{
		const UAnimSequence* Anim = nullptr;
		float Time = 0.0f;
		float Weight = 0.0f;
		bool bLooping = true;
	};

	/** Full-body poses blended by weight (idle, directional walks, crouch, falling). */
	TArray<FSample, TInlineAllocator<12>> Legs;

	/** Replaces the upper body; its weight is the share of the legs that didn't come from the held weapon's own set. */
	FSample UpperBody;
	FName UpperBodyRootBone;
	/** Turn of the upper body about the vertical (degrees, + = character's right); HeadBone turns back by the same. */
	float UpperBodyYaw = 0.0f;
	FName UpperBodyYawHeadBone;

	const UBlendSpace* AimOffset = nullptr;
	float AimOffsetInput = 0.0f;
	float AimOffsetWeight = 1.0f;

	/** Upper-body actions (fire, reload, equip): additive, or replacing the upper body if not additive. */
	TArray<FSample, TInlineAllocator<4>> Actions;

	/**
	 * Hand IK onto the weapon. The weapon's grip point is AnchorOffset (aim space) from AnchorBone; the hand targets
	 * are relative to that point, in aim space. AimRotation is in component space.
	 */
	float HandIKWeight = 0.0f;
	FQuat AimRotation = FQuat::Identity;
	FName AnchorBone;
	FVector AnchorOffset = FVector::ZeroVector;
	FVector RightHandTarget = FVector::ZeroVector;
	FVector LeftHandTarget = FVector::ZeroVector;
	FQuat RightHandRotation = FQuat::Identity;
	FQuat LeftHandRotation = FQuat::Identity;
	bool bLeftHand = false;
	/** The left hand keeps its place relative to the right hand from the animation; LeftHandTarget is then a nudge. */
	bool bLeftHandFollowsRight = false;
	FName RightArmBones[3];
	FName LeftArmBones[3];

	/** Hand shapes from the weapon's rig (replace the target positions above when valid) and the body's finger bones. */
	FTHHandShape RightShape;
	FTHHandShape LeftShape;
	FVector RightShoulderOffset = FVector::ZeroVector;
	FVector LeftShoulderOffset = FVector::ZeroVector;

	/**
	 * Lowered carry: the weapon rides the right hand; the left hand is pulled onto the weapon's left grip by this weight.
	 * The weapon's place comes from GunFromRightHand applied to the posed right hand.
	 */
	float LoweredLeftWeight = 0.0f;
	FTransform GunFromRightHand = FTransform::Identity;
	FVector LeftPalmInGun = FVector::ZeroVector;
	FQuat LeftFrameInGun = FQuat::Identity;
	FQuat HoldRotation = FQuat::Identity;
	FVector LoweredLeftOffset = FVector::ZeroVector;
	FQuat LoweredLeftTurn = FQuat::Identity;

	TArray<FName> RightFingers[FTHHandShape::NumFingers];
	TArray<FName> LeftFingers[FTHHandShape::NumFingers];

	/** Fingers copied from this frame (bones below the hand bones), weighted by HandIKWeight. */
	FSample HandPose;
	bool bHandPoseRight = false;
	bool bHandPoseLeft = false;
};

/** Where the held weapon sits and where its hands go. Renewed by the weapon every frame. */
struct FTHWeaponGrip
{
	/** Body bone the weapon is placed from (the head, so the stock sits at the shoulder and the sights at the eye). */
	FName AnchorBone;
	/** Where the weapon's grip goes, from AnchorBone, in aim space (X forward, Y right, Z up). */
	FVector AnchorOffset = FVector::ZeroVector;
	/** Applied to the aim before placing the weapon. */
	FRotator RotationOffset = FRotator::ZeroRotator;
	/** Hand targets relative to the grip point, in aim space. */
	FVector RightHandTarget = FVector::ZeroVector;
	FVector LeftHandTarget = FVector::ZeroVector;
	/** Hand turns from the animation's angle, about the aim axes. */
	FRotator RightHandRotation = FRotator::ZeroRotator;
	FRotator LeftHandRotation = FRotator::ZeroRotator;
	bool bLeftHand = false;
	/** Left hand keeps the animation's place relative to the right hand (LeftHandTarget becomes a nudge from there). */
	bool bLeftHandFollowsRight = false;
	/** The weapon rig's hands. When valid, the hand targets and rotations above become nudges to these. */
	FTHHandShape RightShape;
	FTHHandShape LeftShape;
	/** Weapon's place relative to the right hand bone, and the rig's left palm in the weapon's own space (unscaled). */
	bool bHasGunFromRightHand = false;
	FTransform GunFromRightHand = FTransform::Identity;
	FVector LeftPalmInGun = FVector::ZeroVector;
	FQuat LeftFrameInGun = FQuat::Identity;
	/** Weapon rotation relative to the aim (its CameraHoldOffset rotation): turns weapon space back into aim axes. */
	FQuat HoldRotation = FQuat::Identity;

	/** Shoulder moves (aim space), made by turning each clavicle before the arms are solved; bends the elbows when the hands stay put. */
	FVector RightShoulderOffset = FVector::ZeroVector;
	FVector LeftShoulderOffset = FVector::ZeroVector;

	/** Aim used for third-person presentation, in the body mesh's component space. */
	static FQuat GetAimInComponentSpace(const APawn* Pawn, const USceneComponent* Body, const FRotator& RotationOffset);
};

struct FTHBodyAnimInstanceProxy : public FAnimInstanceProxy
{
	FTHBodyAnimInstanceProxy() = default;
	explicit FTHBodyAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	/** Written by the game thread in NativeUpdateAnimation, when no worker is using it. */
	FTHBodyPoseSnapshot Snapshot;
};

/**
 * Third-person body animation, written in C++ so it needs no Anim Blueprint: directional walk / crouch for the legs,
 * the held weapon's pose on the upper body, pitch aim offset, fire / reload / equip, and both hands IK'd onto the weapon.
 * What to play comes from UTHBodyAnimSet assets: the current weapon's (UTHWeaponData::BodyAnimSet) and the
 * unarmed one on UTHWeaponComponent, which fills any slot the weapon's set leaves empty.
 * UTHWeaponComponent puts this on the character's body mesh at BeginPlay.
 *
 * Runs on every machine from replicated state (velocity, crouch, aim pitch, equipped weapon, reload), so other
 * players see it. Actions are started by ATHWeaponBase where it already plays effects for everyone.
 */
UCLASS(Transient)
class TACTICALHORROR_API UTHBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Plays an upper-body animation. Duration > 0 stretches it to that many seconds. Restarts it if already playing. */
	void PlayAction(UAnimSequence* Animation, float Duration = 0.0f, float BlendIn = 0.15f, float BlendOut = 0.15f);

	/** Fades out the given action, or all of them if null. */
	void StopAction(const UAnimSequence* Animation = nullptr, float BlendOut = 0.15f);

	/** Where the held weapon sits and its hands go. Call every frame; it lapses if not renewed. */
	void SetWeaponGrip(const FTHWeaponGrip& Grip);

	/** How much the hands are on the weapon's grip points (0 while a full-pose action like reload has them). */
	float GetHandIKAlpha() const { return HandIKAlpha; }

	/** Brings the weapon up from a lowered carry (the held set's LoweredSet) and keeps it up for its ReadyHoldTime. Call on every shot. */
	void RaiseWeapon();

	/** 1 = weapon up (aiming set), 0 = lowered carry. Always 1 for sets without a LoweredSet. */
	float GetReadyAlpha() const { return ReadyAlpha; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	struct FAction
	{
		const UAnimSequence* Anim = nullptr;
		float Position = 0.0f;
		float PlayRate = 1.0f;
		float BlendIn = 0.0f;
		float BlendOut = 0.0f;
		/** Seconds left of a forced fade-out; negative while playing normally. */
		float StopTimeLeft = -1.0f;
		float StopBlend = 0.0f;
	};

	/** Root-track speed in cm/s at play rate 1, measured once per animation. */
	float GetAnimSpeed(const UAnimSequence* Anim);

	/** Returns the strongest weight of a full-pose (non-additive) action, for fading out the hand IK. */
	float UpdateActions(float DeltaSeconds, FTHBodyPoseSnapshot& Out);

	/** Kept alive while sampled. */
	UPROPERTY(Transient)
	TObjectPtr<UTHBodyAnimSet> HeldSet;

	UPROPERTY(Transient)
	TObjectPtr<UTHBodyAnimSet> UnarmedSet;

	float MoveAlpha = 0.0f;
	float CrouchAlpha = 0.0f;
	float FallAlpha = 0.0f;
	float DirectionDegrees = 0.0f;
	float LocomotionPhase = 0.0f;
	float IdleTime = 0.0f;
	float HandIKAlpha = 0.0f;
	float ReadyAlpha = 0.0f;
	float LoweredLeftAlpha = 0.0f;
	/** World time the weapon was last aimed or fired. */
	double LastReadyTime = -1.0e6;

	bool bHasWeaponGrip = false;
	FTHWeaponGrip WeaponGrip;

	/** The body's finger bones, found once per mesh and hand bone. */
	TWeakObjectPtr<const USkeletalMesh> FingersMesh;
	FName FingersHands[2];
	TArray<FName> Fingers[2][FTHHandShape::NumFingers];

	TArray<FAction> Actions;
	TMap<const UAnimSequence*, float> AnimSpeedCache;
};

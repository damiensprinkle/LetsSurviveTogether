#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "THBodyAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;
class UTHBodyAnimSet;

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

	const UBlendSpace* AimOffset = nullptr;
	float AimOffsetInput = 0.0f;

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

	bool bHasWeaponGrip = false;
	FTHWeaponGrip WeaponGrip;

	TArray<FAction> Actions;
	TMap<const UAnimSequence*, float> AnimSpeedCache;
};

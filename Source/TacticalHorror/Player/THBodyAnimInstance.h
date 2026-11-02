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

	FSample UpperBody;
	FName UpperBodyRootBone;

	const UBlendSpace* AimOffset = nullptr;
	float AimOffsetInput = 0.0f;

	/** Upper-body actions (fire, reload, equip): additive, or replacing the upper body if not additive. */
	TArray<FSample, TInlineAllocator<4>> Actions;
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
 * the held weapon's pose layered on the upper body, pitch aim offset, and fire / reload / equip on the upper body.
 * What to play comes from UTHBodyAnimSet assets: the current weapon's (UTHWeaponData::BodyAnimSet) or the
 * unarmed one on UTHWeaponComponent. UTHWeaponComponent puts this on the character's body mesh at BeginPlay.
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

	void UpdateActions(float DeltaSeconds, FTHBodyPoseSnapshot& Out);

	/** Kept alive while sampled. */
	UPROPERTY(Transient)
	TObjectPtr<UTHBodyAnimSet> HeldSet;

	UPROPERTY(Transient)
	TObjectPtr<UTHBodyAnimSet> LegsSet;

	float MoveAlpha = 0.0f;
	float CrouchAlpha = 0.0f;
	float FallAlpha = 0.0f;
	float DirectionDegrees = 0.0f;
	float LocomotionPhase = 0.0f;
	float IdleTime = 0.0f;

	TArray<FAction> Actions;
	TMap<const UAnimSequence*, float> AnimSpeedCache;
};

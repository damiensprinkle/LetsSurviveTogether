#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Enemies/THEnemyAnimSet.h"
#include "THEnemyAnimInstance.generated.h"

class UAnimSequence;

/** One frame's pose for the worker thread: full-body samples blended by weight. Filled on the game thread. */
struct FTHEnemyPoseSnapshot
{
	struct FSample
	{
		const UAnimSequence* Anim = nullptr;
		float Time = 0.0f;
		float Weight = 0.0f;
		bool bLooping = true;
		/** The whole body moved this much (cm, the pelvis in the mesh's space): down for a clip recorded lying on a bed, back
		 *  for a clip that travels (kept in place). */
		FVector Shift = FVector::ZeroVector;
	};
	TArray<FSample, TInlineAllocator<6>> Samples;
	/** Crouched: the arms, neck and head taken back from the zombie's own clips (by ArmsWeight); the crouch gives the legs and
	 *  back. Its idle (Arms) and walk (ArmsMoving) blended by ArmsMovingWeight, so the arms don't jump as it starts or stops. */
	FSample Arms;
	FSample ArmsMoving;
	float ArmsMovingWeight = 0.0f;
	float ArmsWeight = 0.0f;
	/** Upper spine, neck and head bent down this much (degrees) about BendAxis (component space). */
	float HeadDown = 0.0f;
	FVector BendAxis = FVector::ZeroVector;
};

struct FTHEnemyAnimInstanceProxy : public FAnimInstanceProxy
{
	FTHEnemyAnimInstanceProxy() = default;
	explicit FTHEnemyAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	FTHEnemyPoseSnapshot Snapshot;
};

/**
 * Enemy body animation in C++ (no Anim Blueprint): the body's UTHEnemyAnimSet idle, walk and run blended by the
 * capsule's speed (played faster or slower to match it), and attacks over the top. Runs on every machine from
 * replicated movement; attacks come from ATHEnemyCharacter.
 */
UCLASS(Transient, NotBlueprintable)
class TACTICALHORROR_API UTHEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void PlayAttack(const FTHEnemyAttack& Attack);

	bool IsPlayingAttack() const { return AttackAnim != nullptr; }

	/** A stumble: the set's fall, DownTime on the ground, then getting up. */
	void PlayStumble(float DownTime);

	/** A stagger when shot, over the body (under attacks and stumbles). */
	/** StoppedFor: seconds the body stands still for it (its stagger); a whole-body clip is cut to end just after (0: its whole length). */
	void PlayHitReaction(const FTHEnemyAttack& Reaction, float StoppedFor = 0.0f);
	bool IsStumbling() const { return StumblePhase != 0; }
	/** Just up: the pose it got up into is still fading out under its moves (no jump to them). */
	bool IsEasingOutOfStumble() const { return StumblePhase == 0 && StumbleOutAnim != nullptr; }

	/** A stagger (hit reaction) is playing. */
	bool IsPlayingHitReaction() const { return HitAnim != nullptr; }

	/** Lying on the ground from the start until PlayGetUp: the set's Lying Poses[Pose] (its Lying clip, or its Get Up held
	 *  on its start frame), or with no pose the set's Stumble Down (looped; or held on its first frame when it is the
	 *  Stumble Get Up clip too). */
	void PlayLieDown(int32 Pose = INDEX_NONE, bool bBlend = false);

	/** Off the ground into its locomotion (crawling, say), blended. */
	void StopLying();
	/** From lying: the set's Stumble Get Up, then back to standing. */
	void PlayGetUp();

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	/** Kept alive while sampled. */
	UPROPERTY(Transient)
	TObjectPtr<UTHEnemyAnimSet> AnimSet;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> AttackAnim;

	float IdleTime = 0.0f;
	float WalkTime = 0.0f;
	float RunTime = 0.0f;
	float CrouchTime = 0.0f;
	/** Crouching: Ramp goes 0..1 at an even rate, Alpha is it eased in and out (no sudden start or stop). */
	float CrouchRamp = 0.0f;
	float CrouchAlpha = 0.0f;
	/** The body's speed eased, for the idle / walk / run blend: starting and stopping glide instead of snapping. */
	float BlendSpeed = 0.0f;
	float CrouchWalkTime = 0.0f;
	float HeadDown = 0.0f;

	/** Positions in the attack animation (s). */
	float AttackTime = 0.0f;
	float AttackStartTime = 0.0f;
	float AttackEndTime = 0.0f;
	float AttackRate = 1.0f;
	float AttackAlpha = 0.0f;

	/** A stagger when shot: where in its clip, its window and rate, and its weight. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> HitAnim;
	float HitTime = 0.0f;
	float HitStartTime = 0.0f;
	float HitEndTime = 0.0f;
	float HitRate = 1.0f;
	float HitAlpha = 0.0f;

	/** 0 none, 1 falling, 2 down, 3 getting up. */
	int32 StumblePhase = 0;
	float StumbleClipTime = 0.0f;
	float StumbleDownLeft = 0.0f;
	float StumbleAlpha = 0.0f;
	/** Just up (or off the ground): the clip and frame it ended on, held while StumbleAlpha fades out into its moves. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> StumbleOutAnim;
	float StumbleOutTime = 0.0f;
	float StumbleOutLower = 0.0f;

	/** Lying: which of the set's Lying Poses (-1: none, the Stumble Down / Stumble Get Up pair). */
	int32 LyingPose = INDEX_NONE;
	/** Lying, a player near: the pose's Reacting clip blended in (0..1), and where in it. */
	float ReactAlpha = 0.0f;
	float ReactTime = 0.0f;
	/** Getting up from a Lying clip: the lying (and reacting) pose fading out under the get-up (1 -> 0), as they were. */
	float DownFade = 0.0f;
	float DownFadeTime = 0.0f;
	float DownFadeReact = 0.0f;

	const FTHEnemyLyingPose* GetPose() const { return AnimSet && AnimSet->LyingPoses.IsValidIndex(LyingPose) ? &AnimSet->LyingPoses[LyingPose] : nullptr; }
	const FTHEnemyAttack& GetUpClip() const { const FTHEnemyLyingPose* Pose = GetPose(); return Pose ? Pose->GetUp : AnimSet->StumbleGetUp; }

	/** Down is the get-up clip's start frame, held: a pose with no Lying clip, or Stumble Down being the get-up clip itself. */
	bool DownHoldsGetUpStart() const
	{
		const FTHEnemyLyingPose* Pose = GetPose();
		return Pose ? !Pose->Lying : AnimSet && AnimSet->StumbleDown && AnimSet->StumbleDown == AnimSet->StumbleGetUp.Animation;
	}
};

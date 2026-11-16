#include "Enemies/THEnemyAnimInstance.h"
#include "Enemies/THEnemyCharacter.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"

namespace
{
	constexpr float AttackBlendIn = 0.12f;
	constexpr float AttackBlendOut = 0.25f;
	constexpr float AttackBlendInCrouched = 0.3f;
	/** A stagger when shot: quick in, eased out. */
	constexpr float HitBlendIn = 0.08f;
	constexpr float HitBlendOut = 0.25f;
	/** A whole-body flinch is over this long after the body moves again (2026-11-14: it ran on in its flinch, sliding). */
	constexpr float HitMoveOverlap = 0.1f;
	/** Getting up from a lying clip of its own: the lying pose fades out under the get-up over this long. */
	constexpr float GetUpBlendTime = 0.5f;
	/** Getting down or up (developer, 2026-11-13: crouching looked jarring; it took a third of a second at an even rate). */
	constexpr float CrouchBlendTime = 0.6f;
	/** How fast the speed used for the idle / walk / run blend follows the real one (1/s). */
	constexpr float SpeedBlendRate = 6.0f;

	float WrapTime(float Time, const UAnimSequence* Anim)
	{
		const float Length = Anim ? Anim->GetPlayLength() : 0.0f;
		return Length > UE_KINDA_SMALL_NUMBER ? FMath::Fmod(Time, Length) : 0.0f;
	}

	/** Moves the whole body (its pelvis, under which everything hangs): down for a clip recorded lying on a bed, back for one
	 *  that travels. */
	void ShiftBody(FCompactPose& Pose, const FVector& Shift)
	{
		if (Shift.IsNearlyZero())
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(TEXT("pelvis"));
		const FCompactPoseBoneIndex Index = MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (Index.IsValid())
		{
			Pose[Index].AddToTranslation(Shift);
		}
	}
}

float FTHEnemyAttack::GetEnd() const
{
	const float Length = Animation ? Animation->GetPlayLength() : 0.0f;
	return EndTime > 0.0f ? FMath::Min(EndTime, Length) : Length;
}

int32 UTHEnemyAnimSet::PickHitReaction(ETHHitSide Side, bool bHeavy) const
{
	// The side and weight; else the side; else anything
	TArray<int32, TInlineAllocator<8>> Exact, SameSide;
	for (int32 Index = 0; Index < HitReactions.Num(); ++Index)
	{
		const FTHEnemyHitReaction& Each = HitReactions[Index];
		if (!Each.Clip.Animation || Each.Side != Side)
		{
			continue;
		}
		SameSide.Add(Index);
		if (Each.bHeavy == bHeavy)
		{
			Exact.Add(Index);
		}
	}
	const TArray<int32, TInlineAllocator<8>>& From = !Exact.IsEmpty() ? Exact : SameSide;
	if (!From.IsEmpty())
	{
		return From[FMath::RandHelper(From.Num())];
	}
	return HitReactions.IsEmpty() ? INDEX_NONE : FMath::RandHelper(HitReactions.Num());
}

float FTHEnemyAttack::GetDuration() const
{
	return FMath::Max(0.0f, GetEnd() - StartTime) / FMath::Max(PlayRate, 0.1f);
}

float FTHEnemyAttack::GetHitDelay() const
{
	return FMath::Clamp(HitTime - StartTime, 0.0f, GetEnd() - StartTime) / FMath::Max(PlayRate, 0.1f);
}

// Worker thread

bool FTHEnemyAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// Additive clips (Lyra's hit reactions hold only a change from a pose: played as a whole pose, every bone sat at its
	// parent and the body collapsed to a point, so shot enemies vanished; developer, 2026-11-13) go on top of the rest
	TArray<const FTHEnemyPoseSnapshot::FSample*, TInlineAllocator<6>> Base, Additive;
	for (const FTHEnemyPoseSnapshot::FSample& Sample : Snapshot.Samples)
	{
		(Sample.Anim->IsValidAdditive() ? Additive : Base).Add(&Sample);
	}
	const int32 Num = Base.Num();
	if (Num == 0)
	{
		Output.ResetToRefPose();
	}
	else if (Num == 1)
	{
		const FTHEnemyPoseSnapshot::FSample& Sample = *Base[0];
		FAnimationPoseData Data(Output);
		Sample.Anim->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Sample.Time), false, FDeltaTimeRecord(), Sample.bLooping));
		ShiftBody(Output.Pose, Sample.Shift);
	}
	else
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		TArray<FCompactPose, TInlineAllocator<4>> Poses;
		Poses.AddDefaulted(Num);
		TArray<FBlendedCurve, TInlineAllocator<4>> Curves;
		Curves.AddDefaulted(Num);
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<4>> Attributes;
		Attributes.AddDefaulted(Num);
		TArray<float, TInlineAllocator<4>> Weights;

		float TotalWeight = 0.0f;
		for (const FTHEnemyPoseSnapshot::FSample* Sample : Base)
		{
			TotalWeight += Sample->Weight;
		}
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FTHEnemyPoseSnapshot::FSample& Sample = *Base[Index];
			Poses[Index].SetBoneContainer(&Bones);
			Curves[Index].InitFrom(Output.Curve);
			FAnimationPoseData Data(Poses[Index], Curves[Index], Attributes[Index]);
			Sample.Anim->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Sample.Time), false, FDeltaTimeRecord(), Sample.bLooping));
			ShiftBody(Poses[Index], Sample.Shift);
			Weights.Add(Sample.Weight / FMath::Max(TotalWeight, UE_KINDA_SMALL_NUMBER));
		}
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendPosesTogether(TArrayView<const FCompactPose>(Poses), TArrayView<const FBlendedCurve>(Curves),
			TArrayView<const UE::Anim::FStackAttributeContainer>(Attributes), TArrayView<const float>(Weights), OutData);
	}
	for (const FTHEnemyPoseSnapshot::FSample* Sample : Additive)
	{
		FCompactPose AddPose;
		AddPose.SetBoneContainer(&Output.Pose.GetBoneContainer());
		FBlendedCurve AddCurve;
		AddCurve.InitFrom(Output.Curve);
		UE::Anim::FStackAttributeContainer AddAttributes;
		FAnimationPoseData AddData(AddPose, AddCurve, AddAttributes);
		Sample->Anim->GetAnimationPose(AddData, FAnimExtractContext(static_cast<double>(Sample->Time), false, FDeltaTimeRecord(), Sample->bLooping));
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::AccumulateAdditivePose(OutData, AddData, Sample->Weight, Sample->Anim->GetAdditiveAnimType());
		Output.Pose.NormalizeRotations();
	}

	// Crouched: the borrowed crouch's arms (a soldier's, flung out on a zombie) replaced by the zombie clip's own, with
	// the neck and head; each bone's local rotation, so they hang from the crouched shoulders as the zombie holds them
	if (Snapshot.ArmsWeight > 0.001f && Snapshot.Arms.Anim)
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCompactPose ArmsPose;
		ArmsPose.SetBoneContainer(&Bones);
		FBlendedCurve ArmsCurve;
		ArmsCurve.InitFrom(Output.Curve);
		UE::Anim::FStackAttributeContainer ArmsAttributes;
		FAnimationPoseData ArmsData(ArmsPose, ArmsCurve, ArmsAttributes);
		Snapshot.Arms.Anim->GetAnimationPose(ArmsData, FAnimExtractContext(static_cast<double>(Snapshot.Arms.Time), false, FDeltaTimeRecord(), true));
		// Moving: the walk's arms blended in by how much it moves
		const bool bArmsMoving = Snapshot.ArmsMovingWeight > 0.001f && Snapshot.ArmsMoving.Anim;
		FCompactPose MovingPose;
		if (bArmsMoving)
		{
			MovingPose.SetBoneContainer(&Bones);
			FBlendedCurve MovingCurve;
			MovingCurve.InitFrom(Output.Curve);
			UE::Anim::FStackAttributeContainer MovingAttributes;
			FAnimationPoseData MovingData(MovingPose, MovingCurve, MovingAttributes);
			Snapshot.ArmsMoving.Anim->GetAnimationPose(MovingData, FAnimExtractContext(static_cast<double>(Snapshot.ArmsMoving.Time), false, FDeltaTimeRecord(), true));
		}

		// Which bones: the clavicles and neck and everything below them
		const FReferenceSkeleton& Reference = Bones.GetReferenceSkeleton();
		TArray<bool, TInlineAllocator<128>> Take;
		Take.SetNumZeroed(Output.Pose.GetNumBones());
		for (FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
		{
			const FName Name = Reference.GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt());
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
			Take[Index.GetInt()] = Name == TEXT("clavicle_l") || Name == TEXT("clavicle_r") || Name == TEXT("neck_01")
				|| (Parent.IsValid() && Take[Parent.GetInt()]);
			if (Take[Index.GetInt()])
			{
				FTransform& Bone = Output.Pose[Index];
				const FQuat Arm = bArmsMoving ? FQuat::Slerp(ArmsPose[Index].GetRotation(), MovingPose[Index].GetRotation(), Snapshot.ArmsMovingWeight).GetNormalized()
					: ArmsPose[Index].GetRotation();
				Bone.SetRotation(FQuat::Slerp(Bone.GetRotation(), Arm, Snapshot.ArmsWeight).GetNormalized());
			}
		}
	}

	// Head down: the bend shared out from the upper spine to the head, each turned in component space about its own joint
	if (Snapshot.HeadDown > 0.1f && !Snapshot.BendAxis.IsNearlyZero())
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCSPose<FCompactPose> Space;
		Space.InitPose(Output.Pose);
		bool bBent = false;
		for (const TPair<FName, float>& Share : { TPair<FName, float>(TEXT("spine_03"), 0.25f), TPair<FName, float>(TEXT("neck_01"), 0.35f), TPair<FName, float>(TEXT("head"), 0.4f) })
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Share.Key);
			if (MeshIndex == INDEX_NONE)
			{
				continue;
			}
			const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			if (!Index.IsValid())
			{
				continue;
			}
			FTransform Bone = Space.GetComponentSpaceTransform(Index);
			Bone.SetRotation(FQuat(Snapshot.BendAxis, FMath::DegreesToRadians(Snapshot.HeadDown * Share.Value)) * Bone.GetRotation());
			Space.SetComponentSpaceTransform(Index, Bone);
			bBent = true;
		}
		if (bBent)
		{
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Space), Output.Pose);
		}
	}

	// The capsule moves the body, not the animation
	const FCompactPoseBoneIndex RootIndex(0);
	Output.Pose[RootIndex] = Output.Pose.GetRefPose(RootIndex);
	return true;
}

// Game thread

FAnimInstanceProxy* UTHEnemyAnimInstance::CreateAnimInstanceProxy()
{
	return new FTHEnemyAnimInstanceProxy(this);
}

void UTHEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Each enemy starts its loops at a different point, the same on every machine (from where it stands), so a group
	// doesn't sway in step and the server's bones (which shots are traced against) match what players see
	if (const AActor* Owner = GetOwningActor())
	{
		const FVector Location = Owner->GetActorLocation();
		const float Phase = FMath::Frac(FMath::Abs(Location.X * 0.0137 + Location.Y * 0.0291));
		IdleTime = WalkTime = RunTime = CrouchTime = Phase * 10.0f;
	}
}

void UTHEnemyAnimInstance::PlayAttack(const FTHEnemyAttack& Attack)
{
	if (!Attack.Animation || Attack.GetDuration() <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	AttackAnim = Attack.Animation;
	AttackRate = FMath::Max(Attack.PlayRate, 0.1f);
	AttackStartTime = AttackTime = FMath::Min(Attack.StartTime, Attack.GetEnd());
	AttackEndTime = Attack.GetEnd();
}

void UTHEnemyAnimInstance::PlayHitReaction(const FTHEnemyAttack& Reaction, float StoppedFor)
{
	if (!Reaction.Animation || Reaction.GetDuration() <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	HitAnim = Reaction.Animation;
	HitRate = FMath::Max(Reaction.PlayRate, 0.1f);
	HitStartTime = HitTime = FMath::Min(Reaction.StartTime, Reaction.GetEnd());
	HitEndTime = Reaction.GetEnd();
	// A flinch that takes the whole body (not an additive one, which rides on top of its moves) is cut to the time it
	// stands still for: the clips are longer than the stagger, and it set off again with its legs still in the flinch
	if (StoppedFor > 0.0f && !Reaction.Animation->IsValidAdditive())
	{
		HitEndTime = FMath::Min(HitEndTime, HitStartTime + (StoppedFor + HitMoveOverlap) * HitRate);
	}
}

void UTHEnemyAnimInstance::PlayStumble(float DownTime)
{
	if (!AnimSet || !AnimSet->StumbleFall.Animation)
	{
		return;
	}
	StumblePhase = 1;
	LyingPose = INDEX_NONE;
	DownFade = 0.0f;
	StumbleClipTime = AnimSet->StumbleFall.StartTime;
	StumbleDownLeft = DownTime;
	AttackAnim = nullptr;
	StumbleOutAnim = nullptr;
}

void UTHEnemyAnimInstance::PlayLieDown(int32 Pose, bool bBlend)
{
	// The enemy's body set now (it may have just changed: up from crawling, or arrived by replication after this)
	if (const ATHEnemyCharacter* Enemy = Cast<ATHEnemyCharacter>(GetOwningActor()))
	{
		AnimSet = Enemy->GetAnimations();
	}
	if (!AnimSet)
	{
		return;
	}
	LyingPose = AnimSet->LyingPoses.IsValidIndex(Pose) ? Pose : INDEX_NONE;
	if (LyingPose == INDEX_NONE && !AnimSet->StumbleDown)
	{
		return;
	}
	// Already lying there (no fall): on the ground until told to get up
	StumblePhase = 2;
	const FTHEnemyLyingPose* Lying = GetPose();
	StumbleClipTime = DownHoldsGetUpStart() ? GetUpClip().StartTime : Lying && Lying->HoldsLying() ? Lying->LyingHoldTime : 0.0f;
	StumbleDownLeft = 1.0e9f;
	// Already lying there, or dropping into it (a collapse: blended in over a quarter second)
	StumbleAlpha = bBlend ? StumbleAlpha : 1.0f;
	ReactAlpha = 0.0f;
	DownFade = 0.0f;
	AttackAnim = nullptr;
	StumbleOutAnim = nullptr;
}

void UTHEnemyAnimInstance::StopLying()
{
	if (StumblePhase == 2)
	{
		// The pose it lay in stays under the fade (see StumbleOutAnim)
		const FTHEnemyLyingPose* Pose = GetPose();
		StumbleOutAnim = !AnimSet ? nullptr : Pose ? (Pose->Lying ? Pose->Lying.Get() : Pose->GetUp.Animation.Get()) : AnimSet->StumbleDown.Get();
		StumbleOutTime = StumbleOutAnim ? FMath::Fmod(StumbleClipTime, FMath::Max(StumbleOutAnim->GetPlayLength(), UE_KINDA_SMALL_NUMBER)) : 0.0f;
		StumbleOutLower = Pose && Pose->Lying ? Pose->LyingLowerBy : 0.0f;
		StumblePhase = 0;
		DownFade = 0.0f;
	}
}

void UTHEnemyAnimInstance::PlayGetUp()
{
	if (StumblePhase == 2)
	{
		StumbleDownLeft = 0.0f;
	}
}

void UTHEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ATHEnemyCharacter* Enemy = Cast<ATHEnemyCharacter>(GetOwningActor());
	AnimSet = Enemy ? Enemy->GetAnimations() : nullptr;

	FTHEnemyPoseSnapshot Snapshot;
	if (AnimSet && AnimSet->Idle)
	{
		// Attack over everything, faded in and out
		AttackAlpha = 0.0f;
		if (AttackAnim)
		{
			AttackTime += DeltaSeconds * AttackRate;
			const float Remaining = (AttackEndTime - AttackTime) / AttackRate;
			if (Remaining <= 0.0f)
			{
				AttackAnim = nullptr;
			}
			else
			{
				const float Elapsed = (AttackTime - AttackStartTime) / AttackRate;
				// Crouched, rising into a standing attack takes longer (it popped up)
				const float BlendIn = FMath::Lerp(AttackBlendIn, AttackBlendInCrouched, CrouchAlpha);
				AttackAlpha = FMath::Clamp(FMath::Min(Elapsed / BlendIn, Remaining / AttackBlendOut), 0.0f, 1.0f);
			}
		}

		// A stumble over everything: down, a while on the ground, back up
		const UAnimSequence* StumbleAnim = nullptr;
		const FTHEnemyLyingPose* Pose = GetPose();
		const FTHEnemyAttack& GetUp = GetUpClip();
		// Lying with a player near: the pose's Reacting clip (a wave) eased in and out (replicated beckoning)
		ReactTime += DeltaSeconds;
		const bool bReacting = StumblePhase == 2 && Pose && Pose->Reacting && Enemy->IsBeckoning();
		ReactAlpha = FMath::FInterpConstantTo(ReactAlpha, bReacting ? 1.0f : 0.0f, DeltaSeconds, 2.5f);
		if (StumblePhase == 1)
		{
			StumbleClipTime += DeltaSeconds * FMath::Max(AnimSet->StumbleFall.PlayRate, 0.1f);
			if (StumbleClipTime >= AnimSet->StumbleFall.GetEnd())
			{
				StumblePhase = AnimSet->StumbleDown ? 2 : 3;
				StumbleClipTime = StumblePhase == 2 && !DownHoldsGetUpStart() ? 0.0f : GetUp.StartTime;
			}
		}
		else if (StumblePhase == 2)
		{
			// Lying as the get-up starts (the same clip): held still on its first frame
			// A body playing dead: held on its frame
			StumbleClipTime = Pose && Pose->HoldsLying() ? Pose->LyingHoldTime : StumbleClipTime + (DownHoldsGetUpStart() ? 0.0f : DeltaSeconds);
			StumbleDownLeft -= DeltaSeconds;
			if (StumbleDownLeft <= 0.0f)
			{
				// From a lying clip of its own (and maybe a wave): that pose fades out under the get-up, as it was
				// (it used to jump straight to the get-up's first frame)
				if (Pose && (Pose->Lying || ReactAlpha > 0.001f))
				{
					DownFade = 1.0f;
					DownFadeTime = StumbleClipTime;
					DownFadeReact = ReactAlpha;
				}
				StumblePhase = 3;
				StumbleClipTime = GetUp.StartTime;
			}
		}
		else if (StumblePhase == 3)
		{
			StumbleClipTime += DeltaSeconds * FMath::Max(GetUp.PlayRate, 0.1f);
			// No get-up clip (a siren standing or sitting in wait): the pose just eases out (DownFade) into its moves
			if (GetUp.Animation ? StumbleClipTime >= GetUp.GetEnd() : DownFade <= 0.001f)
			{
				// The get-up's last frame stays under the fade into its moves. It used to be dropped here, and with the
				// moves the only pose left the body jumped to them in one frame (2026-11-14)
				StumbleOutAnim = GetUp.Animation;
				StumbleOutTime = GetUp.GetEnd();
				StumbleOutLower = 0.0f;
				StumblePhase = 0;
			}
		}
		DownFade = StumblePhase == 3 ? FMath::FInterpConstantTo(DownFade, 0.0f, DeltaSeconds, 1.0f / GetUpBlendTime) : 0.0f;
		const UAnimSequence* DownAnim = Pose ? (Pose->Lying ? Pose->Lying.Get() : GetUp.Animation.Get()) : AnimSet->StumbleDown.Get();
		StumbleAnim = StumblePhase == 1 ? AnimSet->StumbleFall.Animation.Get() : StumblePhase == 2 ? DownAnim
			: StumblePhase == 3 ? GetUp.Animation.Get() : nullptr;
		const bool bStumbleLoops = StumblePhase == 2 && !DownHoldsGetUpStart() && !(Pose && Pose->HoldsLying());
		const float DownLower = StumblePhase == 2 && Pose && Pose->Lying ? Pose->LyingLowerBy : 0.0f;
		StumbleAlpha = FMath::FInterpConstantTo(StumbleAlpha, StumblePhase != 0 ? 1.0f : 0.0f, DeltaSeconds, 4.0f);
		if (StumblePhase != 0 || StumbleAlpha <= 0.001f)
		{
			StumbleOutAnim = nullptr;
		}
		if (StumblePhase == 3 && !GetUp.Animation)
		{
			StumbleAlpha = FMath::Min(StumbleAlpha, DownFade);
		}
		AttackAlpha *= 1.0f - StumbleAlpha;

		// Idle, walk and run by speed; walk and run play at the speed the body moves
		// Eased: the AI stops and starts in a frame or two, and the clips used to snap from one to the other
		BlendSpeed = FMath::FInterpTo(BlendSpeed, static_cast<float>(Enemy->GetVelocity().Size2D()), DeltaSeconds, SpeedBlendRate);
		const float Speed = BlendSpeed;
		// Pacing calmly (a hunter): a person's walk instead of the zombie's
		const bool bCalm = Enemy->IsCalmWalking() && AnimSet->CalmWalk;
		const UAnimSequence* Walk = bCalm ? AnimSet->CalmWalk.Get() : AnimSet->Walk ? AnimSet->Walk.Get() : nullptr;
		const float WalkAnimSpeed = bCalm ? AnimSet->CalmWalkAnimSpeed : AnimSet->WalkAnimSpeed;
		const UAnimSequence* Run = AnimSet->Run ? AnimSet->Run.Get() : Walk;
		const float RunAnimSpeed = AnimSet->Run ? AnimSet->RunAnimSpeed : WalkAnimSpeed;
		const float WalkFull = FMath::Min(WalkAnimSpeed, 120.0f);
		const float RunStart = WalkAnimSpeed * 1.3f;
		const float RunFull = FMath::Max(RunStart + 1.0f, RunAnimSpeed * 0.8f);
		const float Moving = Walk ? FMath::Clamp(Speed / WalkFull, 0.0f, 1.0f) : 0.0f;
		const float Running = Moving * FMath::Clamp((Speed - RunStart) / (RunFull - RunStart), 0.0f, 1.0f);

		IdleTime += DeltaSeconds;
		CrouchTime += DeltaSeconds;
		// Crouched (replicated), standing still
		CrouchRamp = FMath::FInterpConstantTo(CrouchRamp, Enemy->bIsCrouched && AnimSet->Crouch ? 1.0f : 0.0f, DeltaSeconds, 1.0f / CrouchBlendTime);
		CrouchAlpha = FMath::SmoothStep(0.0f, 1.0f, CrouchRamp);
		WalkTime += DeltaSeconds * FMath::Clamp(Speed / WalkAnimSpeed, 0.6f, 2.0f);
		RunTime += DeltaSeconds * FMath::Clamp(Speed / RunAnimSpeed, 0.6f, FMath::Max(AnimSet->MaxRunPlayRate, 1.0f));

		// A stagger when shot: over the body, eased in and out, under attacks and stumbles
		HitAlpha = 0.0f;
		if (HitAnim)
		{
			HitTime += DeltaSeconds * HitRate;
			const float Remaining = (HitEndTime - HitTime) / HitRate;
			if (Remaining <= 0.0f)
			{
				HitAnim = nullptr;
			}
			else
			{
				const float Elapsed = (HitTime - HitStartTime) / HitRate;
				HitAlpha = FMath::Clamp(FMath::Min(Elapsed / HitBlendIn, Remaining / HitBlendOut), 0.0f, 1.0f) * (1.0f - AttackAlpha) * (1.0f - StumbleAlpha);
			}
		}
		// An additive stagger goes on top of the body's pose (it doesn't replace it)
		const bool bHitOnTop = HitAnim && HitAnim->IsValidAdditive();
		const float Locomotion = (1.0f - AttackAlpha) * (1.0f - StumbleAlpha) * (bHitOnTop ? 1.0f : 1.0f - HitAlpha);
		auto Add = [&Snapshot](const UAnimSequence* Anim, float Time, float Weight, bool bLooping, float Lower = 0.0f, const FVector& Travel = FVector::ZeroVector)
		{
			if (Anim && Weight > UE_KINDA_SMALL_NUMBER)
			{
				// A clip that travels (Travel: its pelvis's way over one loop) held in place: moved back as far as it has gone
				const float At = bLooping ? WrapTime(Time, Anim) : Time;
				const float Length = Anim->GetPlayLength();
				const FVector Back = Length > UE_KINDA_SMALL_NUMBER ? -Travel * (At / Length) : FVector::ZeroVector;
				Snapshot.Samples.Add({ Anim, At, Weight, bLooping, FVector(0.0f, 0.0f, -Lower) + Back });
			}
		};
		Add(AnimSet->Idle, IdleTime, Locomotion * (1.0f - Moving) * (1.0f - CrouchAlpha), true);
		Add(AnimSet->Crouch, CrouchTime, Locomotion * (1.0f - Moving) * CrouchAlpha, true);
		// Crouched, moving is the crouch walk instead (if the set has one)
		const float CrouchMove = AnimSet->CrouchWalk ? CrouchAlpha : 0.0f;
		CrouchWalkTime += DeltaSeconds * FMath::Clamp(Speed / AnimSet->CrouchWalkAnimSpeed, 0.5f, 2.0f);
		const FVector WalkTravel = bCalm ? FVector::ZeroVector : AnimSet->WalkTravel;
		Add(Walk, WalkTime, Locomotion * (Moving - Running) * (1.0f - CrouchMove), true, 0.0f, WalkTravel);
		Add(Run, RunTime, Locomotion * Running * (1.0f - CrouchMove), true, 0.0f, AnimSet->Run ? AnimSet->RunTravel : WalkTravel);
		Add(AnimSet->CrouchWalk, CrouchWalkTime, Locomotion * Moving * CrouchMove, true);
		Add(AttackAnim, AttackTime, AttackAlpha, false);
		Add(HitAnim, HitTime, HitAlpha, false);
		// Lying: the pose, the wave over it by ReactAlpha; getting up: the get-up, with the lying pose fading out under it
		const float Down = StumblePhase == 3 ? DownFade : 0.0f;
		const float Wave = StumblePhase == 2 ? ReactAlpha : StumblePhase == 3 ? DownFadeReact : 0.0f;
		Add(StumbleAnim, StumbleClipTime, StumbleAlpha * (StumblePhase == 2 ? 1.0f - Wave : 1.0f - Down), bStumbleLoops, DownLower);
		// Up (or off the ground): the pose it ended in, fading out under its moves
		Add(StumbleOutAnim, StumbleOutTime, StumbleAlpha, false, StumbleOutLower);
		if (Pose && Down > 0.001f)
		{
			const UAnimSequence* LyingAnim = Pose->Lying ? Pose->Lying.Get() : GetUp.Animation.Get();
			Add(LyingAnim, Pose->Lying ? DownFadeTime : GetUp.StartTime, StumbleAlpha * Down * (1.0f - Wave), Pose->Lying && !Pose->HoldsLying(), Pose->Lying ? Pose->LyingLowerBy : 0.0f);
		}
		if (Pose && Pose->Reacting && Wave > 0.001f)
		{
			Add(Pose->Reacting, ReactTime, StumbleAlpha * Wave * (StumblePhase == 3 ? Down : 1.0f), true, Pose->ReactingLowerBy);
		}

		// Crouched, its arms, neck and head are the zombie's own: its idle's when still, its walk's as it moves, blended by
		// how much it moves (it used to switch at once, and the arms jumped)
		if (CrouchAlpha > 0.001f)
		{
			Snapshot.Arms = { AnimSet->Idle.Get(), WrapTime(IdleTime, AnimSet->Idle), 1.0f, true };
			if (Walk)
			{
				Snapshot.ArmsMoving = { Walk, WrapTime(WalkTime, Walk), 1.0f, true };
				Snapshot.ArmsMovingWeight = Moving;
			}
			Snapshot.ArmsWeight = CrouchAlpha * Locomotion;
		}
	}
	// Head down (an ambusher waiting), eased; about the body's right-hand axis in the mesh's space
	if (Enemy)
	{
		HeadDown = FMath::FInterpConstantTo(HeadDown, Enemy->GetHeadDown(), DeltaSeconds, 70.0f);
		if (HeadDown > 0.1f)
		{
			const FTransform& Mesh = Enemy->GetMesh()->GetComponentTransform();
			const FVector Forward = Mesh.InverseTransformVectorNoScale(Enemy->GetActorForwardVector());
			const FVector Down = Mesh.InverseTransformVectorNoScale(-FVector::UpVector);
			Snapshot.HeadDown = HeadDown;
			Snapshot.BendAxis = FVector::CrossProduct(Forward, Down).GetSafeNormal();
		}
	}
	GetProxyOnGameThread<FTHEnemyAnimInstanceProxy>().Snapshot = MoveTemp(Snapshot);
}

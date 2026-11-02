#include "Player/THBodyAnimInstance.h"
#include "Player/THBodyAnimSet.h"
#include "Weapons/THWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	/** Weight of a walk animation for the movement direction (degrees, 0 = forward, 90 = right). */
	float CardinalWeight(float Direction, float Center)
	{
		return FMath::Max(0.0f, 1.0f - FMath::Abs(FRotator::NormalizeAxis(Direction - Center)) / 90.0f);
	}

	float WrapTime(float Time, const UAnimSequence* Anim)
	{
		const float Length = Anim->GetPlayLength();
		return Length > UE_KINDA_SMALL_NUMBER ? FMath::Fmod(Time, Length) : 0.0f;
	}

	void SampleSequence(const FTHBodyPoseSnapshot::FSample& Sample, FPoseContext& Out)
	{
		FAnimationPoseData Data(Out);
		Sample.Anim->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Sample.Time), false, FDeltaTimeRecord(), Sample.bLooping));
	}

	void ComputeComponentSpace(const FCompactPose& Pose, TArray<FTransform>& OutTransforms)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		OutTransforms.SetNumUninitialized(Pose.GetNumBones());
		for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
			OutTransforms[Index.GetInt()] = Parent.IsValid() ? Pose[Index] * OutTransforms[Parent.GetInt()] : Pose[Index];
		}
	}

	/** Adds an additive pose to the bones in Mask only. */
	void AccumulateMasked(FPoseContext& Output, FPoseContext& Additive, float Weight, EAdditiveAnimationType Type, const TArray<bool>& Mask)
	{
		const FCompactPose Before = Output.Pose;
		FAnimationPoseData BaseData(Output);
		const FAnimationPoseData AdditiveData(Additive);
		FAnimationRuntime::AccumulateAdditivePose(BaseData, AdditiveData, Weight, Type);
		for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
		{
			if (!Mask[Index.GetInt()])
			{
				Output.Pose[Index] = Before[Index];
			}
		}
		Output.Pose.NormalizeRotations();
	}
}

// Worker thread

bool FTHBodyAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();

	const int32 NumLegs = Snapshot.Legs.Num();
	if (NumLegs == 0)
	{
		Output.ResetToRefPose();
		return true;
	}

	// Legs: weighted blend of every locomotion sample
	if (NumLegs == 1)
	{
		SampleSequence(Snapshot.Legs[0], Output);
	}
	else
	{
		TArray<FCompactPose, TInlineAllocator<12>> Poses;
		Poses.AddZeroed(NumLegs);
		TArray<FBlendedCurve, TInlineAllocator<12>> Curves;
		Curves.AddZeroed(NumLegs);
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<12>> Attributes;
		Attributes.AddZeroed(NumLegs);
		TArray<float, TInlineAllocator<12>> Weights;

		float TotalWeight = 0.0f;
		for (const FTHBodyPoseSnapshot::FSample& Sample : Snapshot.Legs)
		{
			TotalWeight += Sample.Weight;
		}

		for (int32 Index = 0; Index < NumLegs; ++Index)
		{
			const FTHBodyPoseSnapshot::FSample& Sample = Snapshot.Legs[Index];
			Poses[Index].SetBoneContainer(&Bones);
			Curves[Index].InitFrom(Output.Curve);
			FAnimationPoseData Data(Poses[Index], Curves[Index], Attributes[Index]);
			Sample.Anim->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Sample.Time), false, FDeltaTimeRecord(), Sample.bLooping));
			Weights.Add(Sample.Weight / FMath::Max(TotalWeight, UE_KINDA_SMALL_NUMBER));
		}

		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendPosesTogether(TArrayView<const FCompactPose>(Poses), TArrayView<const FBlendedCurve>(Curves),
			TArrayView<const UE::Anim::FStackAttributeContainer>(Attributes), TArrayView<const float>(Weights), OutData);
	}

	// The capsule moves the character, not the animation: keep the root where the mesh expects it
	const FCompactPoseBoneIndex RootIndex(0);
	Pose[RootIndex] = Pose.GetRefPose(RootIndex);

	// Upper-body mask: the root bone and its descendants (compact poses list parents before children)
	TArray<bool> Mask;
	Mask.SetNumZeroed(Pose.GetNumBones());
	FCompactPoseBoneIndex UpperRoot(INDEX_NONE);
	if (!Snapshot.UpperBodyRootBone.IsNone())
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Snapshot.UpperBodyRootBone);
		if (MeshIndex != INDEX_NONE)
		{
			UpperRoot = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		}
	}
	if (!UpperRoot.IsValid())
	{
		return true;
	}
	for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
		Mask[Index.GetInt()] = Index == UpperRoot || (Parent.IsValid() && Mask[Parent.GetInt()]);
	}

	// Upper body: the held pose replaces the walk's arms and torso. The torso keeps the pose's facing in mesh space,
	// so it stays pointed forward whatever the hips are doing.
	if (Snapshot.UpperBody.Anim && Snapshot.UpperBody.Weight > 0.0f)
	{
		FPoseContext Upper(Output);
		SampleSequence(Snapshot.UpperBody, Upper);

		TArray<FTransform> LegsSpace;
		TArray<FTransform> UpperSpace;
		ComputeComponentSpace(Pose, LegsSpace);
		ComputeComponentSpace(Upper.Pose, UpperSpace);

		const float Weight = FMath::Clamp(Snapshot.UpperBody.Weight, 0.0f, 1.0f);
		for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
		{
			if (!Mask[Index.GetInt()])
			{
				continue;
			}

			FTransform Target = Upper.Pose[Index];
			if (Index == UpperRoot)
			{
				const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
				const FQuat ParentRotation = Parent.IsValid() ? LegsSpace[Parent.GetInt()].GetRotation() : FQuat::Identity;
				Target.SetRotation(ParentRotation.Inverse() * UpperSpace[Index.GetInt()].GetRotation());
				Target.SetTranslation(Pose[Index].GetTranslation());
			}

			if (Weight >= 1.0f)
			{
				Pose[Index] = Target;
			}
			else
			{
				Pose[Index].BlendWith(Target, Weight);
			}
		}
		Pose.NormalizeRotations();
	}

	// Aim offset: pitch only, additive on the upper body
	if (Snapshot.AimOffset)
	{
		TArray<FBlendSampleData> Samples;
		int32 CachedTriangle = INDEX_NONE;
		if (Snapshot.AimOffset->GetSamplesFromBlendInput(FVector(0.0f, Snapshot.AimOffsetInput, 0.0f), Samples, CachedTriangle, false) && Samples.Num() > 0)
		{
			for (FBlendSampleData& Sample : Samples)
			{
				Sample.Time = 0.0f;
			}

			const UAnimSequence* First = Snapshot.AimOffset->GetBlendSample(Samples[0].SampleDataIndex).Animation;
			const EAdditiveAnimationType Type = First ? First->GetAdditiveAnimType() : AAT_None;
			if (Type != AAT_None)
			{
				FPoseContext Additive(Output, true);
				FAnimationPoseData AdditiveData(Additive);
				Snapshot.AimOffset->GetAnimationPose(Samples, FAnimExtractContext(0.0), AdditiveData);
				AccumulateMasked(Output, Additive, 1.0f, Type, Mask);
			}
		}
	}

	// Actions
	for (const FTHBodyPoseSnapshot::FSample& Action : Snapshot.Actions)
	{
		const EAdditiveAnimationType Type = Action.Anim->GetAdditiveAnimType();
		if (Type != AAT_None)
		{
			FPoseContext Additive(Output, true);
			SampleSequence(Action, Additive);
			AccumulateMasked(Output, Additive, Action.Weight, Type, Mask);
		}
		else
		{
			FPoseContext Full(Output);
			SampleSequence(Action, Full);
			for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
			{
				if (Mask[Index.GetInt()])
				{
					Pose[Index].BlendWith(Full.Pose[Index], Action.Weight);
				}
			}
			Pose.NormalizeRotations();
		}
	}

	return true;
}

// Game thread

FAnimInstanceProxy* UTHBodyAnimInstance::CreateAnimInstanceProxy()
{
	return new FTHBodyAnimInstanceProxy(this);
}

void UTHBodyAnimInstance::PlayAction(UAnimSequence* Animation, float Duration, float BlendIn, float BlendOut)
{
	if (!Animation || Animation->GetPlayLength() <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	Actions.RemoveAll([Animation](const FAction& Action) { return Action.Anim == Animation; });

	FAction& Action = Actions.AddDefaulted_GetRef();
	Action.Anim = Animation;
	Action.PlayRate = Duration > 0.0f ? Animation->GetPlayLength() / Duration : 1.0f;
	Action.BlendIn = BlendIn;
	Action.BlendOut = BlendOut;
}

void UTHBodyAnimInstance::StopAction(const UAnimSequence* Animation, float BlendOut)
{
	for (FAction& Action : Actions)
	{
		if ((!Animation || Action.Anim == Animation) && Action.StopTimeLeft < 0.0f)
		{
			Action.StopBlend = FMath::Max(BlendOut, UE_KINDA_SMALL_NUMBER);
			Action.StopTimeLeft = Action.StopBlend;
		}
	}
}

float UTHBodyAnimInstance::GetAnimSpeed(const UAnimSequence* Anim)
{
	if (const float* Cached = AnimSpeedCache.Find(Anim))
	{
		return *Cached;
	}

	const double Length = Anim->GetPlayLength();
	const FVector Start = Anim->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr).GetTranslation();
	const FVector End = Anim->ExtractRootTrackTransform(FAnimExtractContext(Length), nullptr).GetTranslation();
	const float Speed = Length > UE_KINDA_SMALL_NUMBER ? static_cast<float>((End - Start).Size2D() / Length) : 0.0f;
	AnimSpeedCache.Add(Anim, Speed);
	return Speed;
}

void UTHBodyAnimInstance::UpdateActions(float DeltaSeconds, FTHBodyPoseSnapshot& Out)
{
	for (int32 Index = Actions.Num() - 1; Index >= 0; --Index)
	{
		FAction& Action = Actions[Index];
		Action.Position += DeltaSeconds * Action.PlayRate;
		if (Action.StopTimeLeft >= 0.0f)
		{
			Action.StopTimeLeft -= DeltaSeconds;
		}

		const float Length = Action.Anim->GetPlayLength();
		if (Action.Position >= Length || (Action.StopBlend > 0.0f && Action.StopTimeLeft <= 0.0f))
		{
			Actions.RemoveAt(Index);
			continue;
		}

		// Blend times are in real seconds, positions in animation time
		const float Elapsed = Action.Position / Action.PlayRate;
		const float Remaining = (Length - Action.Position) / Action.PlayRate;
		float Weight = FMath::Min(Action.BlendIn > 0.0f ? Elapsed / Action.BlendIn : 1.0f, Action.BlendOut > 0.0f ? Remaining / Action.BlendOut : 1.0f);
		if (Action.StopTimeLeft >= 0.0f)
		{
			Weight *= Action.StopTimeLeft / Action.StopBlend;
		}

		Out.Actions.Add({ Action.Anim, Action.Position, FMath::Clamp(Weight, 0.0f, 1.0f), false });
	}
}

void UTHBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	FTHBodyPoseSnapshot NewSnapshot;
	ON_SCOPE_EXIT
	{
		GetProxyOnGameThread<FTHBodyAnimInstanceProxy>().Snapshot = MoveTemp(NewSnapshot);
	};

	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	const UTHWeaponComponent* Weapons = Character ? Character->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	if (!Weapons)
	{
		return;
	}

	UTHBodyAnimSet* NewHeldSet = Weapons->GetHeldBodyAnimSet();
	UTHBodyAnimSet* Unarmed = Weapons->GetUnarmedBodyAnimSet();
	if (NewHeldSet != HeldSet)
	{
		// Keep only actions of the new weapon (its equip has usually just started)
		Actions.RemoveAll([NewHeldSet](const FAction& Action)
		{
			return !NewHeldSet || (Action.Anim != NewHeldSet->Equip && Action.Anim != NewHeldSet->Fire && Action.Anim != NewHeldSet->Reload);
		});
		HeldSet = NewHeldSet;
	}
	LegsSet = HeldSet && HeldSet->HasLocomotion() ? HeldSet.Get() : Unarmed;
	const UTHBodyAnimSet* PoseSet = HeldSet ? HeldSet.Get() : Unarmed;
	if (!LegsSet || !LegsSet->HasLocomotion())
	{
		return;
	}

	// Movement state, all replicated for other players' characters
	const FVector Velocity = Character->GetVelocity();
	const FVector LocalVelocity = Character->GetActorRotation().UnrotateVector(FVector(Velocity.X, Velocity.Y, 0.0f));
	const float Speed = LocalVelocity.Size2D();
	if (Speed > 5.0f)
	{
		const float Target = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
		const float Delta = FRotator::NormalizeAxis(Target - DirectionDegrees);
		DirectionDegrees = FRotator::NormalizeAxis(DirectionDegrees + FMath::Clamp(Delta, -720.0f * DeltaSeconds, 720.0f * DeltaSeconds));
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bFalling = Movement && Movement->IsFalling() && LegsSet->Falling;
	MoveAlpha = FMath::FInterpConstantTo(MoveAlpha, Speed > 10.0f ? 1.0f : 0.0f, DeltaSeconds, 6.0f);
	CrouchAlpha = FMath::FInterpConstantTo(CrouchAlpha, Character->bIsCrouched ? 1.0f : 0.0f, DeltaSeconds, 5.0f);
	FallAlpha = FMath::FInterpConstantTo(FallAlpha, bFalling ? 1.0f : 0.0f, DeltaSeconds, 6.0f);
	IdleTime += DeltaSeconds;

	// Legs
	const float Ground = 1.0f - FallAlpha;
	const UAnimSequence* CrouchIdle = LegsSet->CrouchIdle ? LegsSet->CrouchIdle.Get() : LegsSet->Idle.Get();
	const bool bCrouchWalks = LegsSet->CrouchForward != nullptr;

	auto AddLeg = [&NewSnapshot](const UAnimSequence* Anim, float Time, float Weight)
	{
		if (Anim && Weight > 0.001f)
		{
			NewSnapshot.Legs.Add({ Anim, Time, Weight, true });
		}
	};

	AddLeg(LegsSet->Idle, WrapTime(IdleTime, LegsSet->Idle), Ground * (1.0f - MoveAlpha) * (1.0f - CrouchAlpha));
	AddLeg(CrouchIdle, WrapTime(IdleTime, CrouchIdle), Ground * (1.0f - MoveAlpha) * CrouchAlpha);

	struct FDirectional { const UAnimSequence* Anim; float Weight; };
	const float DirectionWeights[4] = { CardinalWeight(DirectionDegrees, 0.0f), CardinalWeight(DirectionDegrees, 180.0f),
		CardinalWeight(DirectionDegrees, -90.0f), CardinalWeight(DirectionDegrees, 90.0f) };
	const UAnimSequence* Stand[4] = { LegsSet->WalkForward, LegsSet->WalkBackward, LegsSet->WalkLeft, LegsSet->WalkRight };
	const UAnimSequence* Crouch[4] = { LegsSet->CrouchForward, LegsSet->CrouchBackward, LegsSet->CrouchLeft, LegsSet->CrouchRight };

	TArray<FDirectional, TInlineAllocator<8>> Walks;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Moving = Ground * MoveAlpha * DirectionWeights[Index];
		Walks.Add({ Stand[Index], Moving * (bCrouchWalks ? 1.0f - CrouchAlpha : 1.0f) });
		if (bCrouchWalks)
		{
			Walks.Add({ Crouch[Index], Moving * CrouchAlpha });
		}
	}

	// One shared phase keeps the feet of blended walks in step; it advances at the speed-matched rate
	float WalkWeight = 0.0f;
	float LengthSum = 0.0f;
	float SpeedSum = 0.0f;
	for (const FDirectional& Walk : Walks)
	{
		if (Walk.Anim && Walk.Weight > 0.001f)
		{
			WalkWeight += Walk.Weight;
			LengthSum += Walk.Weight * Walk.Anim->GetPlayLength();
			SpeedSum += Walk.Weight * GetAnimSpeed(Walk.Anim);
		}
	}
	if (WalkWeight > 0.001f)
	{
		const float AnimSpeed = SpeedSum / WalkWeight;
		const float CycleLength = LengthSum / WalkWeight;
		const float PlayRate = AnimSpeed > 1.0f
			? FMath::Clamp(Speed / AnimSpeed, LegsSet->PlayRateRange.X, LegsSet->PlayRateRange.Y) : 1.0f;
		LocomotionPhase = FMath::Frac(LocomotionPhase + DeltaSeconds * PlayRate / FMath::Max(CycleLength, 0.1f));

		for (const FDirectional& Walk : Walks)
		{
			if (Walk.Anim)
			{
				AddLeg(Walk.Anim, LocomotionPhase * Walk.Anim->GetPlayLength(), Walk.Weight);
			}
		}
	}

	if (LegsSet->Falling)
	{
		AddLeg(LegsSet->Falling, WrapTime(IdleTime, LegsSet->Falling), FallAlpha);
	}

	// Upper body, aim and actions come from what is held
	if (PoseSet)
	{
		NewSnapshot.UpperBodyRootBone = PoseSet->UpperBodyRootBone;
		if (PoseSet->UpperBodyPose)
		{
			NewSnapshot.UpperBody = { PoseSet->UpperBodyPose, WrapTime(IdleTime, PoseSet->UpperBodyPose), 1.0f, true };
		}
		if (PoseSet->AimOffset)
		{
			const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch), -90.0f, 90.0f);
			NewSnapshot.AimOffset = PoseSet->AimOffset;
			NewSnapshot.AimOffsetInput = Pitch / 90.0f * PoseSet->AimOffsetPitchAtLookUp;
		}
	}

	UpdateActions(DeltaSeconds, NewSnapshot);
}

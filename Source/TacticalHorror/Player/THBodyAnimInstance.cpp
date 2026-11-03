#include "Player/THBodyAnimInstance.h"
#include "Player/THBodyAnimSet.h"
#include "Weapons/THWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TacticalHorror.h"

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

	// Hand IK onto the weapon. The weapon is placed from the anchor bone (the head) on the game thread with the same
	// formula, after this pose is final, so the hands land on its grips.
	if (Snapshot.HandIKWeight > 0.0f)
	{
		auto Find = [&Bones](FName Name)
		{
			const int32 MeshIndex = Name.IsNone() ? INDEX_NONE : Bones.GetPoseBoneIndexForBoneName(Name);
			return MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
		};
		const FCompactPoseBoneIndex Anchor = Find(Snapshot.AnchorBone);
		if (Anchor.IsValid())
		{
			TArray<FTransform> Space;
			ComputeComponentSpace(Pose, Space);
			const FVector GripPoint = Space[Anchor.GetInt()].GetLocation() + Snapshot.AimRotation.RotateVector(Snapshot.AnchorOffset);
			const float Weight = FMath::Clamp(Snapshot.HandIKWeight, 0.0f, 1.0f);

			// Turns a hand rotation about the aim axes
			auto TurnInAim = [&Snapshot = Snapshot](const FQuat& Turn, const FQuat& Rotation)
			{
				return (Snapshot.AimRotation * Turn * Snapshot.AimRotation.Inverse() * Rotation).GetNormalized();
			};

			// Moves a hand to Target (component space) with the given rotation, blended by Weight from the animation's.
			// Returns the hand's new component-space transform. The two arms are separate chains, so one pass over
			// the component-space pose serves both.
			auto SolveArm = [&](const FName (&ArmBones)[3], const FVector& Target, const FQuat& TargetRotation) -> FTransform
			{
				const FCompactPoseBoneIndex Upper = Find(ArmBones[0]);
				const FCompactPoseBoneIndex Lower = Find(ArmBones[1]);
				const FCompactPoseBoneIndex Hand = Find(ArmBones[2]);
				const FCompactPoseBoneIndex UpperParent = Upper.IsValid() ? Bones.GetParentBoneIndex(Upper) : FCompactPoseBoneIndex(INDEX_NONE);
				const FCompactPoseBoneIndex Chest = UpperParent.IsValid() ? Bones.GetParentBoneIndex(UpperParent) : FCompactPoseBoneIndex(INDEX_NONE);
				if (!Lower.IsValid() || !Hand.IsValid() || !Chest.IsValid())
				{
					return FTransform::Identity;
				}

				FTransform RootTransform = Space[Upper.GetInt()];
				FTransform JointTransform = Space[Lower.GetInt()];
				FTransform EndTransform = Space[Hand.GetInt()];
				const FQuat HandRotation = EndTransform.GetRotation();
				const FVector Effector = FMath::Lerp(EndTransform.GetLocation(), Target, Weight);

				// Elbow points down and a little out to the side, whatever the source animation did
				const FVector Shoulder = RootTransform.GetLocation();
				const FVector Outward = (Shoulder - Space[Chest.GetInt()].GetLocation()).GetSafeNormal2D();
				const FVector Pole = (Shoulder + Effector) * 0.5f + (FVector::DownVector * 0.8f + Outward * 0.5f).GetSafeNormal() * 50.0f;

				AnimationCore::SolveTwoBoneIK(RootTransform, JointTransform, EndTransform, Pole, Effector, false, 1.0, 1.0);
				EndTransform.SetRotation(FQuat::Slerp(HandRotation, TargetRotation, Weight).GetNormalized());

				Pose[Upper] = RootTransform.GetRelativeTransform(Space[UpperParent.GetInt()]);
				Pose[Lower] = JointTransform.GetRelativeTransform(RootTransform);
				Pose[Hand] = EndTransform.GetRelativeTransform(JointTransform);
				return EndTransform;
			};

			const FCompactPoseBoneIndex RightHand = Find(Snapshot.RightArmBones[2]);
			const FCompactPoseBoneIndex LeftHand = Find(Snapshot.LeftArmBones[2]);
			if (RightHand.IsValid())
			{
				// The left hand's place relative to the right in the animation, before either moves
				const FTransform LeftFromRight = LeftHand.IsValid() ? Space[LeftHand.GetInt()].GetRelativeTransform(Space[RightHand.GetInt()]) : FTransform::Identity;

				const FTransform NewRight = SolveArm(Snapshot.RightArmBones,
					GripPoint + Snapshot.AimRotation.RotateVector(Snapshot.RightHandTarget),
					TurnInAim(Snapshot.RightHandRotation, Space[RightHand.GetInt()].GetRotation()));

				if (Snapshot.bLeftHand && LeftHand.IsValid())
				{
					if (Snapshot.bLeftHandFollowsRight)
					{
						// Keep the animation's two-handed grip: the left hand rides the right, LeftHandTarget nudges it
						const FTransform Left = LeftFromRight * NewRight;
						SolveArm(Snapshot.LeftArmBones, Left.GetLocation() + Snapshot.AimRotation.RotateVector(Snapshot.LeftHandTarget),
							TurnInAim(Snapshot.LeftHandRotation, Left.GetRotation()));
					}
					else
					{
						SolveArm(Snapshot.LeftArmBones, GripPoint + Snapshot.AimRotation.RotateVector(Snapshot.LeftHandTarget),
							TurnInAim(Snapshot.LeftHandRotation, Space[LeftHand.GetInt()].GetRotation()));
					}
				}
			}
			Pose.NormalizeRotations();
		}

		// Fingers from the hand pose frame: every bone below the chosen hand bones (parents come before children)
		if (Snapshot.HandPose.Anim && (Snapshot.bHandPoseRight || Snapshot.bHandPoseLeft))
		{
			FPoseContext HandPose(Output);
			SampleSequence(Snapshot.HandPose, HandPose);
			const FCompactPoseBoneIndex RightHand = Snapshot.bHandPoseRight ? Find(Snapshot.RightArmBones[2]) : FCompactPoseBoneIndex(INDEX_NONE);
			const FCompactPoseBoneIndex LeftHand = Snapshot.bHandPoseLeft ? Find(Snapshot.LeftArmBones[2]) : FCompactPoseBoneIndex(INDEX_NONE);
			TArray<bool> IsFinger;
			IsFinger.Init(false, Pose.GetNumBones());
			for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
			{
				const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
				if (Parent.IsValid() && (Parent == RightHand || Parent == LeftHand || IsFinger[Parent.GetInt()]))
				{
					IsFinger[Index.GetInt()] = true;
					Pose[Index].BlendWith(HandPose.Pose[Index], Snapshot.HandPose.Weight);
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

	// Sum short steps along the clip rather than end minus start: some loops jump back near their last frame
	const double Length = Anim->GetPlayLength();
	constexpr int32 Steps = 20;
	double Distance = 0.0;
	FVector Previous = Anim->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr).GetTranslation();
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		const FVector Current = Anim->ExtractRootTrackTransform(FAnimExtractContext(Length * 0.98 * Step / Steps), nullptr).GetTranslation();
		Distance += (Current - Previous).Size2D();
		Previous = Current;
	}
	const float Speed = Length > UE_KINDA_SMALL_NUMBER ? static_cast<float>(Distance / (Length * 0.98)) : 0.0f;
	AnimSpeedCache.Add(Anim, Speed);
	UE_LOG(LogTacticalHorror, Log, TEXT("Body anim %s moves at %.0f cm/s (root motion; 0 = in place)"), *Anim->GetName(), Speed);
	return Speed;
}

void UTHBodyAnimInstance::SetWeaponGrip(const FTHWeaponGrip& Grip)
{
	bHasWeaponGrip = true;
	WeaponGrip = Grip;
}

FQuat FTHWeaponGrip::GetAimInComponentSpace(const APawn* Pawn, const USceneComponent* Body, const FRotator& RotationOffset)
{
	// Pitch from the aim, yaw from the actor, as ATHWeaponBase::GetPresentationAim
	const FQuat Aim = FRotator(Pawn->GetBaseAimRotation().Pitch, Pawn->GetActorRotation().Yaw, 0.0f).Quaternion() * RotationOffset.Quaternion();
	return Body->GetComponentQuat().Inverse() * Aim;
}

float UTHBodyAnimInstance::UpdateActions(float DeltaSeconds, FTHBodyPoseSnapshot& Out)
{
	float FullPoseWeight = 0.0f;
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
		Weight = FMath::Clamp(Weight, 0.0f, 1.0f);

		Out.Actions.Add({ Action.Anim, Action.Position, Weight, false });
		if (Action.Anim->GetAdditiveAnimType() == AAT_None)
		{
			FullPoseWeight = FMath::Max(FullPoseWeight, Weight);
		}
	}

	return FullPoseWeight;
}

void UTHBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	FTHBodyPoseSnapshot NewSnapshot;
	ON_SCOPE_EXIT
	{
		GetProxyOnGameThread<FTHBodyAnimInstanceProxy>().Snapshot = MoveTemp(NewSnapshot);
		bHasWeaponGrip = false; // the weapon renews it every frame
	};

	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	const UTHWeaponComponent* Weapons = Character ? Character->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	if (!Weapons)
	{
		return;
	}

	UTHBodyAnimSet* NewHeldSet = Weapons->GetHeldBodyAnimSet();
	UnarmedSet = Weapons->GetUnarmedBodyAnimSet();
	if (NewHeldSet != HeldSet)
	{
		// Keep only actions of the new weapon (its equip has usually just started)
		Actions.RemoveAll([NewHeldSet](const FAction& Action)
		{
			return !NewHeldSet || (Action.Anim != NewHeldSet->Equip && Action.Anim != NewHeldSet->Fire && Action.Anim != NewHeldSet->Reload);
		});
		HeldSet = NewHeldSet;
	}
	const UTHBodyAnimSet* Held = HeldSet;
	const UTHBodyAnimSet* Unarmed = UnarmedSet;
	const UTHBodyAnimSet* PoseSet = Held ? Held : Unarmed;
	if (!PoseSet)
	{
		return;
	}

	// Each slot comes from the held weapon's set if it has one, else from the unarmed set.
	// Weight that came from the held set itself needs no upper-body layer: those animations already hold the weapon.
	auto Pick = [Held, Unarmed](TObjectPtr<UAnimSequence> UTHBodyAnimSet::* Slot, bool& bOutFromHeld) -> const UAnimSequence*
	{
		bOutFromHeld = Held && Held != Unarmed && (Held->*Slot) != nullptr;
		if (bOutFromHeld)
		{
			return Held->*Slot;
		}
		return Unarmed ? (Unarmed->*Slot).Get() : nullptr;
	};

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

	bool bFallFromHeld = false;
	const UAnimSequence* Falling = Pick(&UTHBodyAnimSet::Falling, bFallFromHeld);
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bFalling = Movement && Movement->IsFalling() && Falling;
	MoveAlpha = FMath::FInterpConstantTo(MoveAlpha, Speed > 10.0f ? 1.0f : 0.0f, DeltaSeconds, 6.0f);
	CrouchAlpha = FMath::FInterpConstantTo(CrouchAlpha, Character->bIsCrouched ? 1.0f : 0.0f, DeltaSeconds, 5.0f);
	FallAlpha = FMath::FInterpConstantTo(FallAlpha, bFalling ? 1.0f : 0.0f, DeltaSeconds, 6.0f);
	IdleTime += DeltaSeconds;

	float TotalWeight = 0.0f;
	float HeldWeight = 0.0f;
	auto AddLeg = [&](const UAnimSequence* Anim, float Time, float Weight, bool bFromHeld)
	{
		if (Anim && Weight > 0.001f)
		{
			NewSnapshot.Legs.Add({ Anim, Time, Weight, true });
			TotalWeight += Weight;
			HeldWeight += bFromHeld ? Weight : 0.0f;
		}
	};

	// Idle
	const float Ground = 1.0f - FallAlpha;
	bool bIdleFromHeld = false;
	bool bCrouchIdleFromHeld = false;
	const UAnimSequence* Idle = Pick(&UTHBodyAnimSet::Idle, bIdleFromHeld);
	const UAnimSequence* CrouchIdle = Pick(&UTHBodyAnimSet::CrouchIdle, bCrouchIdleFromHeld);
	if (!CrouchIdle)
	{
		CrouchIdle = Idle;
		bCrouchIdleFromHeld = bIdleFromHeld;
	}
	if (Idle)
	{
		AddLeg(Idle, WrapTime(IdleTime, Idle), Ground * (1.0f - MoveAlpha) * (1.0f - CrouchAlpha), bIdleFromHeld);
	}
	if (CrouchIdle)
	{
		AddLeg(CrouchIdle, WrapTime(IdleTime, CrouchIdle), Ground * (1.0f - MoveAlpha) * CrouchAlpha, bCrouchIdleFromHeld);
	}

	// Directional walks, standing and crouched
	struct FDirectional { const UAnimSequence* Anim; float Weight; bool bFromHeld; float AnimSpeed; };
	const float DirectionWeights[4] = { CardinalWeight(DirectionDegrees, 0.0f), CardinalWeight(DirectionDegrees, 180.0f),
		CardinalWeight(DirectionDegrees, -90.0f), CardinalWeight(DirectionDegrees, 90.0f) };
	TObjectPtr<UAnimSequence> UTHBodyAnimSet::* StandSlots[4] = { &UTHBodyAnimSet::WalkForward, &UTHBodyAnimSet::WalkBackward, &UTHBodyAnimSet::WalkLeft, &UTHBodyAnimSet::WalkRight };
	TObjectPtr<UAnimSequence> UTHBodyAnimSet::* CrouchSlots[4] = { &UTHBodyAnimSet::CrouchForward, &UTHBodyAnimSet::CrouchBackward, &UTHBodyAnimSet::CrouchLeft, &UTHBodyAnimSet::CrouchRight };

	// A set's speed override applies to the animations taken from it
	auto SpeedOf = [this, Held, Unarmed](const UAnimSequence* Anim, bool bFromHeld, bool bCrouch)
	{
		const UTHBodyAnimSet* Source = bFromHeld ? Held : Unarmed;
		const float Override = Source ? (bCrouch ? Source->CrouchAnimSpeed : Source->WalkAnimSpeed) : 0.0f;
		return Override > 0.0f ? Override : GetAnimSpeed(Anim);
	};

	TArray<FDirectional, TInlineAllocator<8>> Walks;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Moving = Ground * MoveAlpha * DirectionWeights[Index];
		if (Moving <= 0.001f)
		{
			continue;
		}

		bool bStandFromHeld = false;
		bool bCrouchFromHeld = false;
		const UAnimSequence* Stand = Pick(StandSlots[Index], bStandFromHeld);
		const UAnimSequence* Crouch = Pick(CrouchSlots[Index], bCrouchFromHeld);
		if (!Crouch)
		{
			Crouch = Stand;
			bCrouchFromHeld = bStandFromHeld;
		}
		if (Stand)
		{
			Walks.Add({ Stand, Moving * (1.0f - CrouchAlpha), bStandFromHeld, SpeedOf(Stand, bStandFromHeld, false) });
		}
		if (Crouch)
		{
			Walks.Add({ Crouch, Moving * CrouchAlpha, bCrouchFromHeld, SpeedOf(Crouch, bCrouchFromHeld, Crouch != Stand) });
		}
	}

	// One shared phase keeps the feet of blended walks in step; it advances at the speed-matched rate
	float WalkWeight = 0.0f;
	float LengthSum = 0.0f;
	float SpeedSum = 0.0f;
	for (const FDirectional& Walk : Walks)
	{
		if (Walk.Weight > 0.001f)
		{
			WalkWeight += Walk.Weight;
			LengthSum += Walk.Weight * Walk.Anim->GetPlayLength();
			SpeedSum += Walk.Weight * Walk.AnimSpeed;
		}
	}
	if (WalkWeight > 0.001f)
	{
		const FVector2D Range = PoseSet->PlayRateRange;
		const float AnimSpeed = SpeedSum / WalkWeight;
		const float CycleLength = LengthSum / WalkWeight;
		const float PlayRate = AnimSpeed > 1.0f ? FMath::Clamp(Speed / AnimSpeed, Range.X, Range.Y) : 1.0f;
		LocomotionPhase = FMath::Frac(LocomotionPhase + DeltaSeconds * PlayRate / FMath::Max(CycleLength, 0.1f));

		for (const FDirectional& Walk : Walks)
		{
			AddLeg(Walk.Anim, LocomotionPhase * Walk.Anim->GetPlayLength(), Walk.Weight, Walk.bFromHeld);
		}
	}

	if (Falling)
	{
		AddLeg(Falling, WrapTime(IdleTime, Falling), FallAlpha, bFallFromHeld);
	}

	// Upper body: the held pose covers whatever share of the legs came from another set
	NewSnapshot.UpperBodyRootBone = PoseSet->UpperBodyRootBone;
	if (PoseSet->UpperBodyPose && TotalWeight > 0.0f)
	{
		const float LayerWeight = 1.0f - HeldWeight / TotalWeight;
		if (LayerWeight > 0.001f)
		{
			NewSnapshot.UpperBody = { PoseSet->UpperBodyPose, WrapTime(IdleTime, PoseSet->UpperBodyPose), LayerWeight, true };
		}
	}
	if (PoseSet->AimOffset)
	{
		const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch), -90.0f, 90.0f);
		NewSnapshot.AimOffset = PoseSet->AimOffset;
		NewSnapshot.AimOffsetInput = Pitch / 90.0f * PoseSet->AimOffsetPitchAtLookUp;
	}

	const float FullPoseActionWeight = UpdateActions(DeltaSeconds, NewSnapshot);

	// Hands onto the weapon, eased in and out so switching weapons or finishing a reload doesn't snap
	const float IKTarget = bHasWeaponGrip && Held ? Held->HandIKWeight * (1.0f - FullPoseActionWeight) : 0.0f;
	HandIKAlpha = FMath::FInterpConstantTo(HandIKAlpha, IKTarget, DeltaSeconds, 8.0f);
	if (HandIKAlpha > 0.001f && bHasWeaponGrip)
	{
		NewSnapshot.HandIKWeight = HandIKAlpha;
		NewSnapshot.AimRotation = FTHWeaponGrip::GetAimInComponentSpace(Character, GetSkelMeshComponent(), WeaponGrip.RotationOffset);
		NewSnapshot.AnchorBone = WeaponGrip.AnchorBone;
		NewSnapshot.AnchorOffset = WeaponGrip.AnchorOffset;
		NewSnapshot.RightHandTarget = WeaponGrip.RightHandTarget;
		NewSnapshot.LeftHandTarget = WeaponGrip.LeftHandTarget;
		NewSnapshot.bLeftHand = WeaponGrip.bLeftHand;
		NewSnapshot.bLeftHandFollowsRight = WeaponGrip.bLeftHandFollowsRight;
		NewSnapshot.RightHandRotation = WeaponGrip.RightHandRotation.Quaternion();
		NewSnapshot.LeftHandRotation = WeaponGrip.LeftHandRotation.Quaternion();
		NewSnapshot.RightArmBones[0] = Held->RightUpperArmBone;
		NewSnapshot.RightArmBones[1] = Held->RightLowerArmBone;
		NewSnapshot.RightArmBones[2] = Held->RightHandBone;
		NewSnapshot.LeftArmBones[0] = Held->LeftUpperArmBone;
		NewSnapshot.LeftArmBones[1] = Held->LeftLowerArmBone;
		NewSnapshot.LeftArmBones[2] = Held->LeftHandBone;
		if (Held->HandPose)
		{
			NewSnapshot.HandPose = { Held->HandPose, FMath::Clamp(Held->HandPoseTime, 0.0f, Held->HandPose->GetPlayLength()), HandIKAlpha, false };
			NewSnapshot.bHandPoseRight = Held->bHandPoseRight;
			NewSnapshot.bHandPoseLeft = Held->bHandPoseLeft && WeaponGrip.bLeftHand;
		}
	}
}


#include "Player/THBodyAnimInstance.h"
#include "Player/THBodyAnimSet.h"
#include "Weapons/THWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "TwoBoneIK.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TacticalHorror.h"

static TAutoConsoleVariable<int32> CVarDebugHandFingers(
	TEXT("th.DebugHandFingers"), 1,
	TEXT("1 = third-person fingers copy the weapon rig's fingers (normal). 0 = keep the body animation's fingers, for comparison."));

static TAutoConsoleVariable<float> CVarUpperBodyYawTune(
	TEXT("th.UpperBodyYawTune"), 0.0f,
	TEXT("Live tuning: degrees added to the held body set's Upper Body Yaw (+ = character's right). Copy the sum into the set."));

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
				AccumulateMasked(Output, Additive, FMath::Clamp(Snapshot.AimOffsetWeight, 0.0f, 1.0f), Type, Mask);
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

	// Square up a bladed torso: turn the upper body about the vertical, and the neck back so the head still looks ahead
	if (!FMath::IsNearlyZero(Snapshot.UpperBodyYaw))
	{
		TArray<FTransform> Space;
		ComputeComponentSpace(Pose, Space);
		const FQuat Turn = FRotator(0.0f, Snapshot.UpperBodyYaw, 0.0f).Quaternion();
		auto TurnBone = [&](FCompactPoseBoneIndex Bone, const FQuat& By)
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
			FTransform BoneSpace = Space[Bone.GetInt()];
			BoneSpace.SetRotation((By * BoneSpace.GetRotation()).GetNormalized());
			Pose[Bone] = Parent.IsValid() ? BoneSpace.GetRelativeTransform(Space[Parent.GetInt()]) : BoneSpace;
		};
		TurnBone(UpperRoot, Turn);
		const int32 HeadMesh = Snapshot.UpperBodyYawHeadBone.IsNone() ? INDEX_NONE : Bones.GetPoseBoneIndexForBoneName(Snapshot.UpperBodyYawHeadBone);
		if (HeadMesh != INDEX_NONE)
		{
			// The neck's parent has turned by Turn, so its component rotation now includes it; undo that for the neck only
			ComputeComponentSpace(Pose, Space);
			TurnBone(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HeadMesh)), Turn.Inverse());
		}
		Pose.NormalizeRotations();
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

			// Shoulders: turn the clavicle so the upper arm's root moves by the offset (aim space). With the hand held on
			// the gun, a shoulder brought closer bends the elbow more.
			auto MoveShoulder = [&](const FName (&ArmBones)[3], const FVector& Offset)
			{
				const FCompactPoseBoneIndex Upper = Find(ArmBones[0]);
				const FCompactPoseBoneIndex Clavicle = Upper.IsValid() ? Bones.GetParentBoneIndex(Upper) : FCompactPoseBoneIndex(INDEX_NONE);
				const FCompactPoseBoneIndex ClavicleParent = Clavicle.IsValid() ? Bones.GetParentBoneIndex(Clavicle) : FCompactPoseBoneIndex(INDEX_NONE);
				if (Offset.IsNearlyZero() || !ClavicleParent.IsValid())
				{
					return false;
				}
				const FVector Root = Space[Clavicle.GetInt()].GetLocation();
				const FVector From = Space[Upper.GetInt()].GetLocation() - Root;
				const FVector To = From + Snapshot.AimRotation.RotateVector(Offset) * Weight;
				const FQuat Turn = FQuat::FindBetweenVectors(From, To);
				FTransform ClavicleSpace = Space[Clavicle.GetInt()];
				ClavicleSpace.SetRotation((Turn * ClavicleSpace.GetRotation()).GetNormalized());
				Pose[Clavicle] = ClavicleSpace.GetRelativeTransform(Space[ClavicleParent.GetInt()]);
				return true;
			};
			const bool bRightShoulder = MoveShoulder(Snapshot.RightArmBones, Snapshot.RightShoulderOffset);
			const bool bLeftShoulder = Snapshot.bLeftHand && MoveShoulder(Snapshot.LeftArmBones, Snapshot.LeftShoulderOffset);
			if (bRightShoulder || bLeftShoulder)
			{
				ComputeComponentSpace(Pose, Space);
			}

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

			// Where the hand bone must go for its palm to match the weapon rig's hand (Shape), nudged by Offset / Turn.
			// The palm frame relative to the hand bone comes from this skeleton's own knuckles, so bone axes don't matter.
			auto ShapeTarget = [&](FCompactPoseBoneIndex Hand, const TArray<FName> (&Fingers)[FTHHandShape::NumFingers],
				const FTHHandShape& Shape, const FVector& Offset, const FQuat& Turn, FVector& OutLocation, FQuat& OutRotation)
			{
				const FCompactPoseBoneIndex Index = Fingers[1].Num() ? Find(Fingers[1][0]) : FCompactPoseBoneIndex(INDEX_NONE);
				const FCompactPoseBoneIndex Middle = Fingers[2].Num() ? Find(Fingers[2][0]) : FCompactPoseBoneIndex(INDEX_NONE);
				const FCompactPoseBoneIndex Pinky = Fingers[4].Num() ? Find(Fingers[4][0]) : FCompactPoseBoneIndex(INDEX_NONE);
				if (!Shape.bValid || !Index.IsValid() || !Middle.IsValid() || !Pinky.IsValid())
				{
					return false;
				}
				const FTransform& HandSpace = Space[Hand.GetInt()];
				const FVector Wrist = HandSpace.GetLocation();
				const FVector I = Space[Index.GetInt()].GetLocation();
				const FVector M = Space[Middle.GetInt()].GetLocation();
				const FVector P = Space[Pinky.GetInt()].GetLocation();
				const FQuat FrameInHand = HandSpace.GetRotation().Inverse() * FTHHandShape::MakeFrame(Wrist, I, M, P);
				const FVector PalmInHand = HandSpace.InverseTransformPosition(FTHHandShape::MakePalm(Wrist, I, M, P));

				const FQuat Frame = TurnInAim(Turn, Snapshot.AimRotation * Shape.Frame);
				OutRotation = (Frame * FrameInHand.Inverse()).GetNormalized();
				const FVector Palm = GripPoint + Snapshot.AimRotation.RotateVector(Shape.Palm + Offset);
				OutLocation = Palm - OutRotation.RotateVector(PalmInHand * HandSpace.GetScale3D());
				return true;
			};

			const FCompactPoseBoneIndex RightHand = Find(Snapshot.RightArmBones[2]);
			const FCompactPoseBoneIndex LeftHand = Find(Snapshot.LeftArmBones[2]);
			if (RightHand.IsValid())
			{
				// The left hand's place relative to the right in the animation, before either moves
				const FTransform LeftFromRight = LeftHand.IsValid() ? Space[LeftHand.GetInt()].GetRelativeTransform(Space[RightHand.GetInt()]) : FTransform::Identity;

				// Both targets are worked out before either arm moves (the arms are separate chains)
				FVector RightLocation;
				FQuat RightRotation;
				if (!ShapeTarget(RightHand, Snapshot.RightFingers, Snapshot.RightShape, Snapshot.RightHandTarget, Snapshot.RightHandRotation, RightLocation, RightRotation))
				{
					RightLocation = GripPoint + Snapshot.AimRotation.RotateVector(Snapshot.RightHandTarget);
					RightRotation = TurnInAim(Snapshot.RightHandRotation, Space[RightHand.GetInt()].GetRotation());
				}
				FVector LeftLocation = FVector::ZeroVector;
				FQuat LeftRotation = FQuat::Identity;
				const bool bLeft = Snapshot.bLeftHand && LeftHand.IsValid();
				const bool bLeftShape = bLeft && !Snapshot.bLeftHandFollowsRight
					&& ShapeTarget(LeftHand, Snapshot.LeftFingers, Snapshot.LeftShape, Snapshot.LeftHandTarget, Snapshot.LeftHandRotation, LeftLocation, LeftRotation);

				const FTransform NewRight = SolveArm(Snapshot.RightArmBones, RightLocation, RightRotation);

				if (bLeftShape)
				{
					SolveArm(Snapshot.LeftArmBones, LeftLocation, LeftRotation);
				}
				else if (bLeft && Snapshot.bLeftHandFollowsRight)
				{
					// Keep the animation's two-handed grip: the left hand rides the right, LeftHandTarget nudges it
					const FTransform Left = LeftFromRight * NewRight;
					SolveArm(Snapshot.LeftArmBones, Left.GetLocation() + Snapshot.AimRotation.RotateVector(Snapshot.LeftHandTarget),
						TurnInAim(Snapshot.LeftHandRotation, Left.GetRotation()));
				}
				else if (bLeft)
				{
					SolveArm(Snapshot.LeftArmBones, GripPoint + Snapshot.AimRotation.RotateVector(Snapshot.LeftHandTarget),
						TurnInAim(Snapshot.LeftHandRotation, Space[LeftHand.GetInt()].GetRotation()));
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

		// Fingers from the weapon rig's hands. Each joint is turned to point the way the rig's does, with its hinge kept
		// along the knuckle line (fingers bend about it), so no joint twists and the next one bends the right way. The
		// thumb hinges differently, so it only gets the least turn that points it.
		const bool bRightFingers = Snapshot.RightShape.bValid;
		const bool bLeftFingers = Snapshot.LeftShape.bValid && Snapshot.bLeftHand && !Snapshot.bLeftHandFollowsRight;
		if (bRightFingers || bLeftFingers)
		{
			TArray<FTransform> Space;
			ComputeComponentSpace(Pose, Space);
			const float Weight = FMath::Clamp(Snapshot.HandIKWeight, 0.0f, 1.0f);
			auto FindBone = [&Bones](FName Name)
			{
				const int32 MeshIndex = Name.IsNone() ? INDEX_NONE : Bones.GetPoseBoneIndexForBoneName(Name);
				return MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
			};
			auto Curl = [&](const TArray<FName> (&Fingers)[FTHHandShape::NumFingers], const FTHHandShape& Shape)
			{
				// Knuckle line of this hand now, and of the rig's hand as placed
				const FCompactPoseBoneIndex IndexKnuckle = Fingers[1].Num() ? FindBone(Fingers[1][0]) : FCompactPoseBoneIndex(INDEX_NONE);
				const FCompactPoseBoneIndex PinkyKnuckle = Fingers[4].Num() ? FindBone(Fingers[4][0]) : FCompactPoseBoneIndex(INDEX_NONE);
				if (!IndexKnuckle.IsValid() || !PinkyKnuckle.IsValid())
				{
					return;
				}
				const FVector HingeNow = (Space[IndexKnuckle.GetInt()].GetLocation() - Space[PinkyKnuckle.GetInt()].GetLocation()).GetSafeNormal();
				const FVector HingeWanted = Snapshot.AimRotation.RotateVector(Shape.Frame.GetAxisY());

				for (int32 Finger = 0; Finger < FTHHandShape::NumFingers; ++Finger)
				{
					const TArray<FName>& Chain = Fingers[Finger];
					// Every joint with a rig direction, the last one included (it points along the axis its parent pointed to it)
					const int32 Count = FMath::Min(Chain.Num(), Shape.NumDirections[Finger]);
					FVector TipAxis = FVector::ZeroVector; // the last joint's length axis, in its parent's local space
					for (int32 Joint = 0; Joint < Count; ++Joint)
					{
						const FCompactPoseBoneIndex Bone = FindBone(Chain[Joint]);
						const FCompactPoseBoneIndex Parent = Bone.IsValid() ? Bones.GetParentBoneIndex(Bone) : FCompactPoseBoneIndex(INDEX_NONE);
						if (!Bone.IsValid() || !Parent.IsValid())
						{
							break;
						}
						const FCompactPoseBoneIndex Child = Joint + 1 < Chain.Num() ? FindBone(Chain[Joint + 1]) : FCompactPoseBoneIndex(INDEX_NONE);
						const bool bHasChild = Child.IsValid() && Bones.GetParentBoneIndex(Child) == Bone;
						FTransform& BoneSpace = Space[Bone.GetInt()];

						FVector Current;
						if (bHasChild)
						{
							Current = (Space[Child.GetInt()].GetLocation() - BoneSpace.GetLocation()).GetSafeNormal();
							TipAxis = BoneSpace.GetRotation().UnrotateVector(Current);
						}
						else
						{
							// Fingertip bone: assume it runs along the same local axis as the bone before it
							Current = BoneSpace.GetRotation().RotateVector(TipAxis).GetSafeNormal();
						}
						const FVector Wanted = Snapshot.AimRotation.RotateVector(Shape.FingerDirections[Finger][Joint]);
						if (Current.IsNearlyZero() || Wanted.IsNearlyZero())
						{
							break;
						}

						FQuat Turn;
						const bool bHinged = Finger != 0 && FMath::Abs(Current | HingeNow) < 0.9f && FMath::Abs(Wanted | HingeWanted) < 0.9f;
						if (bHinged)
						{
							Turn = FRotationMatrix::MakeFromXY(Wanted, HingeWanted).ToQuat() * FRotationMatrix::MakeFromXY(Current, HingeNow).ToQuat().Inverse();
						}
						else
						{
							Turn = FQuat::FindBetweenNormals(Current, Wanted);
						}
						Turn = FQuat::Slerp(FQuat::Identity, Turn, Weight);

						BoneSpace.SetRotation((Turn * BoneSpace.GetRotation()).GetNormalized());
						Pose[Bone] = BoneSpace.GetRelativeTransform(Space[Parent.GetInt()]);
						// The next joint along hangs off this one
						if (bHasChild)
						{
							Space[Child.GetInt()] = Pose[Child] * BoneSpace;
						}
						else
						{
							break;
						}
					}
				}
			};
			if (bRightFingers)
			{
				Curl(Snapshot.RightFingers, Snapshot.RightShape);
			}
			if (bLeftFingers)
			{
				Curl(Snapshot.LeftFingers, Snapshot.LeftShape);
			}
			Pose.NormalizeRotations();
		}
	}

	// Lowered carry: the weapon rides the animation's right hand; pull the left hand onto its left grip (the rig's left
	// palm, as when aiming) so it doesn't float off the gun
	if (Snapshot.LoweredLeftWeight > 0.0f)
	{
		auto Find = [&Bones](FName Name)
		{
			const int32 MeshIndex = Name.IsNone() ? INDEX_NONE : Bones.GetPoseBoneIndexForBoneName(Name);
			return MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
		};
		const FCompactPoseBoneIndex RightHand = Find(Snapshot.RightArmBones[2]);
		const FCompactPoseBoneIndex Upper = Find(Snapshot.LeftArmBones[0]);
		const FCompactPoseBoneIndex Lower = Find(Snapshot.LeftArmBones[1]);
		const FCompactPoseBoneIndex Hand = Find(Snapshot.LeftArmBones[2]);
		const TArray<FName> (&Fingers)[FTHHandShape::NumFingers] = Snapshot.LeftFingers;
		const FCompactPoseBoneIndex Index = Fingers[1].Num() ? Find(Fingers[1][0]) : FCompactPoseBoneIndex(INDEX_NONE);
		const FCompactPoseBoneIndex Middle = Fingers[2].Num() ? Find(Fingers[2][0]) : FCompactPoseBoneIndex(INDEX_NONE);
		const FCompactPoseBoneIndex Pinky = Fingers[4].Num() ? Find(Fingers[4][0]) : FCompactPoseBoneIndex(INDEX_NONE);
		const FCompactPoseBoneIndex UpperParent = Upper.IsValid() ? Bones.GetParentBoneIndex(Upper) : FCompactPoseBoneIndex(INDEX_NONE);
		const FCompactPoseBoneIndex Chest = UpperParent.IsValid() ? Bones.GetParentBoneIndex(UpperParent) : FCompactPoseBoneIndex(INDEX_NONE);
		if (RightHand.IsValid() && Lower.IsValid() && Hand.IsValid() && Chest.IsValid() && Index.IsValid() && Middle.IsValid() && Pinky.IsValid())
		{
			TArray<FTransform> Space;
			ComputeComponentSpace(Pose, Space);
			const float Weight = FMath::Clamp(Snapshot.LoweredLeftWeight, 0.0f, 1.0f);

			// Where the weapon is (as the game thread will place it) and its aim axes
			const FTransform Gun = Snapshot.GunFromRightHand * Space[RightHand.GetInt()];
			const FQuat GunAim = Gun.GetRotation() * Snapshot.HoldRotation.Inverse();

			// The left hand's palm frame relative to its bone, from this skeleton's knuckles
			const FTransform& HandSpace = Space[Hand.GetInt()];
			const FVector Wrist = HandSpace.GetLocation();
			const FVector I = Space[Index.GetInt()].GetLocation();
			const FVector M = Space[Middle.GetInt()].GetLocation();
			const FVector P = Space[Pinky.GetInt()].GetLocation();
			const FQuat FrameInHand = HandSpace.GetRotation().Inverse() * FTHHandShape::MakeFrame(Wrist, I, M, P);
			const FVector PalmInHand = HandSpace.InverseTransformPosition(FTHHandShape::MakePalm(Wrist, I, M, P));

			const FQuat Turn = (GunAim * Snapshot.LoweredLeftTurn * GunAim.Inverse()).GetNormalized();
			const FQuat Frame = (Turn * Gun.GetRotation() * Snapshot.LeftFrameInGun).GetNormalized();
			const FVector Palm = Gun.TransformPosition(Snapshot.LeftPalmInGun) + GunAim.RotateVector(Snapshot.LoweredLeftOffset);
			const FQuat TargetRotation = (Frame * FrameInHand.Inverse()).GetNormalized();
			const FVector Target = Palm - TargetRotation.RotateVector(PalmInHand * HandSpace.GetScale3D());

			FTransform RootTransform = Space[Upper.GetInt()];
			FTransform JointTransform = Space[Lower.GetInt()];
			FTransform EndTransform = HandSpace;
			const FQuat AnimRotation = EndTransform.GetRotation();
			const FVector Effector = FMath::Lerp(EndTransform.GetLocation(), Target, Weight);
			const FVector Shoulder = RootTransform.GetLocation();
			const FVector Outward = (Shoulder - Space[Chest.GetInt()].GetLocation()).GetSafeNormal2D();
			const FVector Pole = (Shoulder + Effector) * 0.5f + (FVector::DownVector * 0.8f + Outward * 0.5f).GetSafeNormal() * 50.0f;
			AnimationCore::SolveTwoBoneIK(RootTransform, JointTransform, EndTransform, Pole, Effector, false, 1.0, 1.0);
			EndTransform.SetRotation(FQuat::Slerp(AnimRotation, TargetRotation, Weight).GetNormalized());

			Pose[Upper] = RootTransform.GetRelativeTransform(Space[UpperParent.GetInt()]);
			Pose[Lower] = JointTransform.GetRelativeTransform(RootTransform);
			Pose[Hand] = EndTransform.GetRelativeTransform(JointTransform);

			// Left fingers from the hand pose, as when aiming
			if (Snapshot.HandPose.Anim && Snapshot.bHandPoseLeft)
			{
				FPoseContext HandPose(Output);
				SampleSequence(Snapshot.HandPose, HandPose);
				TArray<bool> IsFinger;
				IsFinger.Init(false, Pose.GetNumBones());
				for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
				{
					const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
					if (Parent.IsValid() && (Parent == Hand || IsFinger[Parent.GetInt()]))
					{
						IsFinger[Bone.GetInt()] = true;
						Pose[Bone].BlendWith(HandPose.Pose[Bone], Weight);
					}
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

void UTHBodyAnimInstance::RaiseWeapon()
{
	if (const UWorld* World = GetWorld())
	{
		LastReadyTime = World->GetTimeSeconds();
	}
}

void UTHBodyAnimInstance::SetWeaponGrip(const FTHWeaponGrip& Grip)
{
	bHasWeaponGrip = true;
	WeaponGrip = Grip;
}

void FTHHandShape::FindFingers(const FReferenceSkeleton& Skeleton, FName Hand, TArray<FName> (&OutFingers)[NumFingers])
{
	static const TCHAR* Keys[NumFingers] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	for (TArray<FName>& Chain : OutFingers)
	{
		Chain.Reset();
	}
	const int32 HandIndex = Skeleton.FindBoneIndex(Hand);
	if (HandIndex == INDEX_NONE)
	{
		return;
	}

	// Reference skeletons list parents before children, so each chain comes out base first
	for (int32 Bone = HandIndex + 1; Bone < Skeleton.GetNum(); ++Bone)
	{
		int32 Parent = Skeleton.GetParentIndex(Bone);
		while (Parent != INDEX_NONE && Parent != HandIndex)
		{
			Parent = Skeleton.GetParentIndex(Parent);
		}
		if (Parent != HandIndex)
		{
			continue;
		}
		const FString Name = Skeleton.GetBoneName(Bone).ToString();
		if (Name.Contains(TEXT("metacarpal")))
		{
			continue;
		}
		for (int32 Finger = 0; Finger < NumFingers; ++Finger)
		{
			if (Name.Contains(Keys[Finger]) && OutFingers[Finger].Num() < MaxJoints + 1)
			{
				OutFingers[Finger].Add(Skeleton.GetBoneName(Bone));
				break;
			}
		}
	}
}

FQuat FTHHandShape::MakeFrame(const FVector& Wrist, const FVector& Index, const FVector& Middle, const FVector& Pinky)
{
	const FVector Forward = (Middle - Wrist).GetSafeNormal();
	const FVector Across = (Index - Pinky).GetSafeNormal();
	return FRotationMatrix::MakeFromXY(Forward, Across).ToQuat();
}

FVector FTHHandShape::MakePalm(const FVector& Wrist, const FVector& Index, const FVector& Middle, const FVector& Pinky)
{
	// The knuckles: hands of different sizes then wrap the grip from the same line (the first-person hands are larger)
	return (Index + Middle + Pinky) / 3.0f;
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

	// Weapon up (the held set) or lowered (its LoweredSet): up while aiming and for ReadyHoldTime after the last shot
	const UTHBodyAnimSet* Lowered = Held && Held->LoweredSet && Held->LoweredSet != Held ? Held->LoweredSet.Get() : nullptr;
	if (Lowered)
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Weapons->IsAiming())
		{
			LastReadyTime = Now;
		}
		const float ReadyTarget = Now - LastReadyTime < Held->ReadyHoldTime ? 1.0f : 0.0f;
		const float BlendTime = ReadyTarget > ReadyAlpha ? Held->RaiseTime : Held->LowerTime;
		ReadyAlpha = FMath::FInterpConstantTo(ReadyAlpha, ReadyTarget, DeltaSeconds, 1.0f / FMath::Max(BlendTime, 0.01f));
	}
	else
	{
		ReadyAlpha = 1.0f;
	}

	// The weapon sets blended by readiness: their slots are picked from each, weighted
	struct FSetShare { const UTHBodyAnimSet* Set; float Weight; };
	TArray<FSetShare, TInlineAllocator<2>> Shares;
	if (Lowered && ReadyAlpha < 0.999f)
	{
		Shares.Add({ Lowered, 1.0f - ReadyAlpha });
	}
	if (!Lowered || ReadyAlpha > 0.001f)
	{
		Shares.Add({ Held, Lowered ? ReadyAlpha : 1.0f });
	}

	// Each slot comes from the weapon's set if it has one, else from the unarmed set.
	// Weight that came from a weapon set itself needs no upper-body layer: those animations already hold the weapon.
	auto PickFrom = [Unarmed](const UTHBodyAnimSet* Set, TObjectPtr<UAnimSequence> UTHBodyAnimSet::* Slot, bool& bOutFromHeld) -> const UAnimSequence*
	{
		bOutFromHeld = Set && Set != Unarmed && (Set->*Slot) != nullptr;
		if (bOutFromHeld)
		{
			return Set->*Slot;
		}
		return Unarmed ? (Unarmed->*Slot).Get() : nullptr;
	};
	auto Pick = [&PickFrom, Held](TObjectPtr<UAnimSequence> UTHBodyAnimSet::* Slot, bool& bOutFromHeld)
	{
		return PickFrom(Held, Slot, bOutFromHeld);
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

	// Idle, from each weapon set by its share
	const float Ground = 1.0f - FallAlpha;
	for (const FSetShare& Share : Shares)
	{
		bool bIdleFromHeld = false;
		bool bCrouchIdleFromHeld = false;
		const UAnimSequence* Idle = PickFrom(Share.Set, &UTHBodyAnimSet::Idle, bIdleFromHeld);
		const UAnimSequence* CrouchIdle = PickFrom(Share.Set, &UTHBodyAnimSet::CrouchIdle, bCrouchIdleFromHeld);
		if (!CrouchIdle)
		{
			CrouchIdle = Idle;
			bCrouchIdleFromHeld = bIdleFromHeld;
		}
		if (Idle)
		{
			AddLeg(Idle, WrapTime(IdleTime, Idle), Share.Weight * Ground * (1.0f - MoveAlpha) * (1.0f - CrouchAlpha), bIdleFromHeld);
		}
		if (CrouchIdle)
		{
			AddLeg(CrouchIdle, WrapTime(IdleTime, CrouchIdle), Share.Weight * Ground * (1.0f - MoveAlpha) * CrouchAlpha, bCrouchIdleFromHeld);
		}
	}

	// Directional walks, standing and crouched
	struct FDirectional { const UAnimSequence* Anim; float Weight; bool bFromHeld; float AnimSpeed; };
	const float DirectionWeights[4] = { CardinalWeight(DirectionDegrees, 0.0f), CardinalWeight(DirectionDegrees, 180.0f),
		CardinalWeight(DirectionDegrees, -90.0f), CardinalWeight(DirectionDegrees, 90.0f) };
	TObjectPtr<UAnimSequence> UTHBodyAnimSet::* StandSlots[4] = { &UTHBodyAnimSet::WalkForward, &UTHBodyAnimSet::WalkBackward, &UTHBodyAnimSet::WalkLeft, &UTHBodyAnimSet::WalkRight };
	TObjectPtr<UAnimSequence> UTHBodyAnimSet::* CrouchSlots[4] = { &UTHBodyAnimSet::CrouchForward, &UTHBodyAnimSet::CrouchBackward, &UTHBodyAnimSet::CrouchLeft, &UTHBodyAnimSet::CrouchRight };

	// A set's speed override applies to the animations taken from it
	auto SpeedOf = [this, Unarmed](const UTHBodyAnimSet* Set, const UAnimSequence* Anim, bool bFromHeld, bool bCrouch)
	{
		const UTHBodyAnimSet* Source = bFromHeld ? Set : Unarmed;
		const float Override = Source ? (bCrouch ? Source->CrouchAnimSpeed : Source->WalkAnimSpeed) : 0.0f;
		return Override > 0.0f ? Override : GetAnimSpeed(Anim);
	};

	TArray<FDirectional, TInlineAllocator<8>> Walks;
	for (const FSetShare& Share : Shares)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const float Moving = Share.Weight * Ground * MoveAlpha * DirectionWeights[Index];
			if (Moving <= 0.001f)
			{
				continue;
			}

			bool bStandFromHeld = false;
			bool bCrouchFromHeld = false;
			const UAnimSequence* Stand = PickFrom(Share.Set, StandSlots[Index], bStandFromHeld);
			const UAnimSequence* Crouch = PickFrom(Share.Set, CrouchSlots[Index], bCrouchFromHeld);
			if (!Crouch)
			{
				Crouch = Stand;
				bCrouchFromHeld = bStandFromHeld;
			}
			if (Stand)
			{
				Walks.Add({ Stand, Moving * (1.0f - CrouchAlpha), bStandFromHeld, SpeedOf(Share.Set, Stand, bStandFromHeld, false) });
			}
			if (Crouch)
			{
				Walks.Add({ Crouch, Moving * CrouchAlpha, bCrouchFromHeld, SpeedOf(Share.Set, Crouch, bCrouchFromHeld, Crouch != Stand) });
			}
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
	NewSnapshot.UpperBodyYaw = FMath::Lerp(Lowered ? Lowered->UpperBodyYaw : PoseSet->UpperBodyYaw, PoseSet->UpperBodyYaw, ReadyAlpha)
		+ CVarUpperBodyYawTune.GetValueOnGameThread();
	NewSnapshot.UpperBodyYawHeadBone = PoseSet->UpperBodyYawHeadBone;
	// While lowered, the lowered set's own pose layers over borrowed legs (e.g. the unarmed crouch), if it has one
	const UTHBodyAnimSet* LayerSet = Lowered && ReadyAlpha < 0.5f && Lowered->UpperBodyPose ? Lowered : PoseSet;
	if (LayerSet->UpperBodyPose && TotalWeight > 0.0f)
	{
		const float LayerWeight = 1.0f - HeldWeight / TotalWeight;
		if (LayerWeight > 0.001f)
		{
			NewSnapshot.UpperBody = { LayerSet->UpperBodyPose, WrapTime(IdleTime, LayerSet->UpperBodyPose), LayerWeight, true };
		}
	}
	if (PoseSet->AimOffset)
	{
		// Aiming up and down belongs to the raised weapon; a lowered carry doesn't follow the view
		const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch), -90.0f, 90.0f);
		NewSnapshot.AimOffset = PoseSet->AimOffset;
		NewSnapshot.AimOffsetInput = Pitch / 90.0f * PoseSet->AimOffsetPitchAtLookUp;
		NewSnapshot.AimOffsetWeight = ReadyAlpha;
	}

	const float FullPoseActionWeight = UpdateActions(DeltaSeconds, NewSnapshot);

	// Hands onto the weapon, eased in and out so switching weapons or finishing a reload doesn't snap
	// Lowered, the weapon rides the right hand of the lowered animation instead
	const float IKTarget = bHasWeaponGrip && Held ? Held->HandIKWeight * (1.0f - FullPoseActionWeight) * ReadyAlpha : 0.0f;
	HandIKAlpha = FMath::FInterpConstantTo(HandIKAlpha, IKTarget, DeltaSeconds, 8.0f);
	// Lowered, the left hand still goes onto the weapon (which rides the right hand)
	const float LoweredLeftTarget = bHasWeaponGrip && Held && Lowered && Lowered->bLeftHandOnWeapon && WeaponGrip.bHasGunFromRightHand && WeaponGrip.bLeftHand
		&& !WeaponGrip.bLeftHandFollowsRight ? (1.0f - ReadyAlpha) * (1.0f - FullPoseActionWeight) : 0.0f;
	LoweredLeftAlpha = FMath::FInterpConstantTo(LoweredLeftAlpha, LoweredLeftTarget, DeltaSeconds, 8.0f);
	if ((HandIKAlpha > 0.001f || LoweredLeftAlpha > 0.001f) && bHasWeaponGrip)
	{
		NewSnapshot.LoweredLeftWeight = LoweredLeftAlpha;
		NewSnapshot.GunFromRightHand = WeaponGrip.GunFromRightHand;
		NewSnapshot.LeftPalmInGun = WeaponGrip.LeftPalmInGun;
		NewSnapshot.LeftFrameInGun = WeaponGrip.LeftFrameInGun;
		NewSnapshot.HoldRotation = WeaponGrip.HoldRotation;
		NewSnapshot.LoweredLeftOffset = WeaponGrip.LeftHandTarget;
		NewSnapshot.LoweredLeftTurn = WeaponGrip.LeftHandRotation.Quaternion();
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

		// The body's finger bones, for matching the weapon rig's hands
		const USkeletalMesh* BodyMesh = GetSkelMeshComponent()->GetSkeletalMeshAsset();
		if (BodyMesh && (FingersMesh.Get() != BodyMesh || FingersHands[0] != Held->RightHandBone || FingersHands[1] != Held->LeftHandBone))
		{
			FingersMesh = BodyMesh;
			FingersHands[0] = Held->RightHandBone;
			FingersHands[1] = Held->LeftHandBone;
			FTHHandShape::FindFingers(BodyMesh->GetRefSkeleton(), FingersHands[0], Fingers[0]);
			FTHHandShape::FindFingers(BodyMesh->GetRefSkeleton(), FingersHands[1], Fingers[1]);
		}
		NewSnapshot.RightShoulderOffset = WeaponGrip.RightShoulderOffset;
		NewSnapshot.LeftShoulderOffset = WeaponGrip.LeftShoulderOffset;
		NewSnapshot.RightShape = WeaponGrip.RightShape;
		NewSnapshot.LeftShape = WeaponGrip.LeftShape;
		if (CVarDebugHandFingers.GetValueOnGameThread() != 0)
		{
			for (int32 Finger = 0; Finger < FTHHandShape::NumFingers; ++Finger)
			{
				NewSnapshot.RightFingers[Finger] = Fingers[0][Finger];
				NewSnapshot.LeftFingers[Finger] = Fingers[1][Finger];
			}
		}
		else
		{
			// Only the knuckles are needed to place the hands
			for (int32 Finger = 0; Finger < FTHHandShape::NumFingers; ++Finger)
			{
				NewSnapshot.RightFingers[Finger] = Fingers[0][Finger];
				NewSnapshot.LeftFingers[Finger] = Fingers[1][Finger];
				NewSnapshot.RightShape.NumDirections[Finger] = 0;
				NewSnapshot.LeftShape.NumDirections[Finger] = 0;
			}
		}

		// A hand covered by an authored Hand Pose keeps those fingers instead of the copied ones
		if (Held->HandPose)
		{
			for (int32 Finger = 0; Finger < FTHHandShape::NumFingers; ++Finger)
			{
				NewSnapshot.RightShape.NumDirections[Finger] = Held->bHandPoseRight ? 0 : NewSnapshot.RightShape.NumDirections[Finger];
				NewSnapshot.LeftShape.NumDirections[Finger] = Held->bHandPoseLeft ? 0 : NewSnapshot.LeftShape.NumDirections[Finger];
			}
		}
		if (Held->HandPose)
		{
			NewSnapshot.HandPose = { Held->HandPose, FMath::Clamp(Held->HandPoseTime, 0.0f, Held->HandPose->GetPlayLength()), HandIKAlpha, false };
			NewSnapshot.bHandPoseRight = Held->bHandPoseRight;
			NewSnapshot.bHandPoseLeft = Held->bHandPoseLeft && WeaponGrip.bLeftHand;
		}
	}
}


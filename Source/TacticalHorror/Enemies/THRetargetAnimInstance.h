#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "THRetargetAnimInstance.generated.h"

class UIKRetargeter;

/** Runs one retarget node: the pose of another mesh (the source), copied onto this one through an IK Retargeter. */
USTRUCT()
struct FTHRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FTHRetargetAnimInstanceProxy() = default;
	explicit FTHRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InNode)
		: FAnimInstanceProxy(InAnimInstance), Node(InNode) {}

	virtual FAnimNode_Base* GetCustomRootNode() override { return Node; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(Node); }

	FAnimNode_RetargetPoseFromMesh* Node = nullptr;
};

/**
 * A body on another skeleton than the enemy's animations (a MetaHuman): copies the pose of the enemy's own animated
 * mesh (hidden; ATHEnemyCharacter::ApplyAppearance) through the look's IK Retargeter, every frame, ragdoll included. No
 * Anim Blueprint.
 */
UCLASS(Transient, NotBlueprintable)
class TACTICALHORROR_API UTHRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Where the pose comes from, and how it maps onto this body. */
	void SetSource(UIKRetargeter* Retargeter, USkeletalMeshComponent* Source);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	FAnimNode_RetargetPoseFromMesh Node;
};

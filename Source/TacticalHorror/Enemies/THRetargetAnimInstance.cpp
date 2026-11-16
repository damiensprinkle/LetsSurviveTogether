#include "Enemies/THRetargetAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Retargeter/IKRetargeter.h"

FAnimInstanceProxy* UTHRetargetAnimInstance::CreateAnimInstanceProxy()
{
	Node.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	return new FTHRetargetAnimInstanceProxy(this, &Node);
}

void UTHRetargetAnimInstance::SetSource(UIKRetargeter* Retargeter, USkeletalMeshComponent* Source)
{
	Node.IKRetargeterAsset = Retargeter;
	Node.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	Node.SourceMeshComponent = Source;
	if (FIKRetargetProcessor* Processor = Node.GetRetargetProcessor())
	{
		Processor->SetNeedsInitialized();
	}
}

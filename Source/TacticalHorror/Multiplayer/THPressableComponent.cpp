#include "Multiplayer/THPressableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

UTHPressableComponent::UTHPressableComponent()
{
	SetIsReplicatedByDefault(true);
}

bool UTHPressableComponent::IsWithinReach(const AActor* Player) const
{
	return Player && FVector::Dist(Player->GetActorLocation(), GetOwner()->GetActorLocation()) <= MaxPressDistance;
}

void UTHPressableComponent::DisableBlueprintOverlap(FName ComponentName) const
{
	if (UPrimitiveComponent* Primitive = FindOwnerComponent<UPrimitiveComponent>(ComponentName))
	{
		Primitive->SetGenerateOverlapEvents(false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: %s has no component %s, so its Blueprint input stays on."), *GetClass()->GetName(), *GetOwner()->GetName(), *ComponentName.ToString());
	}
}

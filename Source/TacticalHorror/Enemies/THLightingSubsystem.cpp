#include "Enemies/THLightingSubsystem.h"

#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/** Shining now: visible, bright, its actor not hidden. */
	bool IsShining(const ULightComponent* Light)
	{
		const AActor* Owner = Light ? Light->GetOwner() : nullptr;
		return Light && Light->IsVisible() && Light->Intensity > 0.0f && (!Owner || !Owner->IsHidden());
	}

	/** Carried by a pawn (a player's or an enemy's light), or by something attached to one. */
	bool IsCarried(const ULightComponent* Light)
	{
		for (const AActor* Owner = Light->GetOwner(); Owner; Owner = Owner->GetAttachParentActor())
		{
			if (Owner->IsA<APawn>() || Owner->GetOwner() && Owner->GetOwner()->IsA<APawn>())
			{
				return true;
			}
		}
		return false;
	}
}

float UTHLightingSubsystem::GetLightOn(const APawn* Player) const
{
	if (!Player)
	{
		return 0.0f;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const TPair<double, float>* Kept = Cache.Find(Player); Kept && Now - Kept->Key < 0.25)
	{
		return Kept->Value;
	}
	const float Light = Measure(Player);
	Cache.Add(Player, TPair<double, float>(Now, Light));
	return Light;
}

float UTHLightingSubsystem::Measure(const APawn* Player) const
{
	UWorld* World = GetWorld();

	// Their own light on: lit
	TArray<AActor*> Carried;
	Player->GetAttachedActors(Carried, true, true);
	Carried.Add(const_cast<APawn*>(Player));
	for (const AActor* Holder : Carried)
	{
		TInlineComponentArray<ULightComponent*> Lights(Holder);
		for (const ULightComponent* Light : Lights)
		{
			if (IsShining(Light))
			{
				return 1.0f;
			}
		}
	}

	// The level's lamps, listed every few seconds
	const double Now = World->GetTimeSeconds();
	if (Now - LampsListedAt > 5.0)
	{
		LampsListedAt = Now;
		Lamps.Reset();
		for (TObjectIterator<ULightComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsA<ULocalLightComponent>() && !IsCarried(*It))
			{
				Lamps.Add(*It);
			}
		}
	}
	const FVector Chest = Player->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	float Best = 0.0f;
	for (const TWeakObjectPtr<ULightComponent>& Each : Lamps)
	{
		const ULocalLightComponent* Lamp = Cast<ULocalLightComponent>(Each.Get());
		if (!Lamp || !IsShining(Lamp))
		{
			continue;
		}
		const FVector From = Lamp->GetComponentLocation();
		const float Reach = Lamp->AttenuationRadius;
		const float Distance = FVector::Dist(From, Chest);
		if (Reach <= 0.0f || Distance >= Reach)
		{
			continue;
		}
		if (const USpotLightComponent* Spot = Cast<USpotLightComponent>(Lamp))
		{
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Spot->GetForwardVector(), (Chest - From).GetSafeNormal())));
			if (Angle > Spot->OuterConeAngle)
			{
				continue;
			}
		}
		const float Level = 1.0f - Distance / Reach;
		if (Level <= Best)
		{
			continue;
		}
		// A clear line from the lamp to them (the lamp's own fitting doesn't count)
		FCollisionQueryParams Params(SCENE_QUERY_STAT(THLightOnPlayer), false, Player);
		Params.AddIgnoredActor(Lamp->GetOwner());
		if (!World->LineTraceTestByObjectType(From, Chest, Objects, Params))
		{
			Best = Level;
		}
	}
	return Best;
}

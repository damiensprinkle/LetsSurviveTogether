#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "THImpactSurfaces.generated.h"

class UMaterialInterface;

/** What a shot leaves on one kind of surface. */
USTRUCT(BlueprintType)
struct FTHSurfaceImpact
{
	GENERATED_BODY()

	/** Bullet-hole decals; one is picked at random per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TArray<TObjectPtr<UMaterialInterface>> Decals;
};

/**
 * Bullet holes by surface, shared by all guns (UTHWeaponData::ImpactSurfaces). The surface is the Surface Type of the
 * hit material's Physical Material (Project Settings, Physics, Physical Surface). Surfaces not listed use Default.
 * Set up by Scripts/Tools/setup_impact_decals.py.
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHImpactSurfaces : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	FTHSurfaceImpact Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, FTHSurfaceImpact> Surfaces;

	/** A random decal for this surface, or null if there are none. */
	UMaterialInterface* PickDecal(EPhysicalSurface Surface) const;
};

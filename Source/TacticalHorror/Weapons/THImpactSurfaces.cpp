#include "Weapons/THImpactSurfaces.h"

#include "Materials/MaterialInterface.h"

UMaterialInterface* UTHImpactSurfaces::PickDecal(EPhysicalSurface Surface) const
{
	const FTHSurfaceImpact* Impact = Surfaces.Find(Surface);
	if (!Impact || Impact->Decals.IsEmpty())
	{
		Impact = &Default;
	}
	return Impact->Decals.IsEmpty() ? nullptr : Impact->Decals[FMath::RandHelper(Impact->Decals.Num())].Get();
}

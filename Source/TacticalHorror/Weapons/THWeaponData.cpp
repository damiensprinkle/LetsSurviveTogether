#include "Weapons/THWeaponData.h"

FVector2D UTHWeaponData::GetCumulativeRecoil(int32 ShotIndex) const
{
	FVector2D Total = FVector2D::ZeroVector;
	if (RecoilPattern.IsEmpty())
	{
		return Total;
	}

	for (int32 i = 0; i < ShotIndex; ++i)
	{
		Total += RecoilPattern[FMath::Min(i, RecoilPattern.Num() - 1)];
	}

	return Total * RecoilScale;
}

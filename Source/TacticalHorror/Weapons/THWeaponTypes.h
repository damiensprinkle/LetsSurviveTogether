#pragma once

#include "CoreMinimal.h"
#include "THWeaponTypes.generated.h"

UENUM(BlueprintType)
enum class ETHFireMode : uint8
{
	SemiAuto,
	FullAuto
};

UENUM(BlueprintType)
enum class ETHWeaponSlot : uint8
{
	Primary,
	Secondary
};

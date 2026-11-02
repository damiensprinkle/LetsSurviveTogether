#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "THGameInstance.generated.h"

/**
 * Persistent, game-wide state that survives map travel.
 * Session management lives in UTHSessionSubsystem (a GameInstance subsystem), not here.
 */
UCLASS()
class TACTICALHORROR_API UTHGameInstance : public UGameInstance
{
	GENERATED_BODY()
};

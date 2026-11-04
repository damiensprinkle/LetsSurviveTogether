#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "THMenuGameMode.generated.h"

class UTHMainMenuWidget;

/** Game mode for the main menu map: no character, just the menu player controller. */
UCLASS()
class TACTICALHORROR_API ATHMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATHMenuGameMode();
};

/**
 * Shows the main menu and gives it the mouse.
 * Main Menu Widget Class: a Widget Blueprint based on UTHMainMenuWidget, set in DefaultGame.ini:
 *   [/Script/TacticalHorror.THMenuPlayerController]
 *   MainMenuWidgetClass=/Game/UI/WBP_MainMenu.WBP_MainMenu_C
 * Empty = UTHMainMenuWidget's plain built-in layout.
 */
UCLASS(Config = Game)
class TACTICALHORROR_API ATHMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Menu")
	TSoftClassPtr<UTHMainMenuWidget> MainMenuWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTHMainMenuWidget> MainMenu;
};

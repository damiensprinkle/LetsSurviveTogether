#include "Core/THMenuGameMode.h"
#include "UI/THMainMenuWidget.h"
#include "Blueprint/UserWidget.h"

ATHMenuGameMode::ATHMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ATHMenuPlayerController::StaticClass();
}

void ATHMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	UClass* WidgetClass = MainMenuWidgetClass.IsNull() ? nullptr : MainMenuWidgetClass.LoadSynchronous();
	MainMenu = CreateWidget<UTHMainMenuWidget>(this, WidgetClass ? WidgetClass : UTHMainMenuWidget::StaticClass());
	if (!MainMenu)
	{
		return;
	}
	MainMenu->AddToViewport();

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(MainMenu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

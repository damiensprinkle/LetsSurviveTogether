#include "Player/THPlayerController.h"
#include "UI/THAudioSettingsWidget.h"
#include "UI/THCrosshairWidget.h"
#include "UI/THAmmoWidget.h"
#include "Multiplayer/THSessionSubsystem.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TacticalHorror.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

ATHPlayerController::ATHPlayerController()
{
	AudioSettingsWidgetClass = UTHAudioSettingsWidget::StaticClass();
	CrosshairWidgetClass = UTHCrosshairWidget::StaticClass();
	AmmoWidgetClass = UTHAmmoWidget::StaticClass();
}

void ATHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}
	if (CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UTHCrosshairWidget>(this, CrosshairWidgetClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport(-10);
		}
	}
	if (AmmoWidgetClass)
	{
		AmmoWidget = CreateWidget<UTHAmmoWidget>(this, AmmoWidgetClass);
		if (AmmoWidget)
		{
			AmmoWidget->AddToViewport(-10);
		}
	}
}

void ATHPlayerController::LeaveGame()
{
	CloseAudioSettings();
	if (UTHSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTHSessionSubsystem>() : nullptr)
	{
		Sessions->LeaveSession();
	}
	// Back to the start map (the main menu). For a host this closes the server, and the clients go back to their menu.
	const FString MenuMap = UGameMapsSettings::GetGameDefaultMap();
	UE_LOG(LogTacticalHorror, Log, TEXT("Leaving game for %s"), *MenuMap);
	UGameplayStatics::OpenLevel(this, FName(*FPackageName::ObjectPathToPackageName(MenuMap)));
}

void ATHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Plain key bindings rather than an Enhanced Input context: the pawn Blueprint manages its own contexts and could clear ours.
	// While the menu is open input is UI-only, so the widget handles these keys itself.
	FInputKeyBinding& Escape = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ATHPlayerController::ToggleAudioSettings);
	Escape.bExecuteWhenPaused = true;
	FInputKeyBinding& P = InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ATHPlayerController::ToggleAudioSettings);
	P.bExecuteWhenPaused = true;
}

void ATHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AudioSettingsWidget)
	{
		AudioSettingsWidget->RemoveFromParent();
		AudioSettingsWidget = nullptr;
	}
	if (CrosshairWidget)
	{
		CrosshairWidget->RemoveFromParent();
		CrosshairWidget = nullptr;
	}
	if (AmmoWidget)
	{
		AmmoWidget->RemoveFromParent();
		AmmoWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool ATHPlayerController::IsAudioSettingsOpen() const
{
	return AudioSettingsWidget && AudioSettingsWidget->IsInViewport();
}

void ATHPlayerController::ToggleAudioSettings()
{
	if (IsAudioSettingsOpen())
	{
		CloseAudioSettings();
	}
	else
	{
		OpenAudioSettings();
	}
}

void ATHPlayerController::OpenAudioSettings()
{
	if (!IsLocalController() || !AudioSettingsWidgetClass)
	{
		return;
	}

	if (!AudioSettingsWidget)
	{
		AudioSettingsWidget = CreateWidget<UTHAudioSettingsWidget>(this, AudioSettingsWidgetClass);
		if (!AudioSettingsWidget)
		{
			return;
		}
		AudioSettingsWidget->OnCloseRequested.BindUObject(this, &ATHPlayerController::CloseAudioSettings);
		AudioSettingsWidget->OnLeaveGameRequested.BindUObject(this, &ATHPlayerController::LeaveGame);
		AudioSettingsWidget->bShowLeaveGame = true;
	}

	// Co-op: the game keeps running. Stop a held trigger, since its release won't reach the game while the menu has input.
	if (const APawn* MyPawn = GetPawn())
	{
		if (const UTHWeaponComponent* Weapons = MyPawn->FindComponentByClass<UTHWeaponComponent>())
		{
			if (ATHWeaponBase* Weapon = Weapons->GetCurrentWeapon())
			{
				Weapon->StopFire();
			}
		}
	}
	FlushPressedKeys();

	AudioSettingsWidget->AddToViewport(100);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(AudioSettingsWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void ATHPlayerController::CloseAudioSettings()
{
	if (!AudioSettingsWidget)
	{
		return;
	}

	AudioSettingsWidget->RemoveFromParent();
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	if (UTHAudioSettingsSubsystem* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTHAudioSettingsSubsystem>() : nullptr)
	{
		Settings->SaveSettings();
	}
}

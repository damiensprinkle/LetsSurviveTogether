#include "Player/THPlayerController.h"
#include "UI/THAudioSettingsWidget.h"
#include "UI/THCrosshairWidget.h"
#include "UI/THAmmoWidget.h"
#include "UI/THStatusWidget.h"
#include "Player/THCameraModifier.h"
#include "Player/Components/THSanityComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Multiplayer/THSessionSubsystem.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TacticalHorror.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Settings/THControlsSettingsSubsystem.h"
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
	StatusWidgetClass = UTHStatusWidget::StaticClass();
}

void ATHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}
	// The main menu leaves input in UI-only mode with the cursor showing, and that outlives the map change
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

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
	if (StatusWidgetClass)
	{
		StatusWidget = CreateWidget<UTHStatusWidget>(this, StatusWidgetClass);
		if (StatusWidget)
		{
			StatusWidget->AddToViewport(5);
		}
	}
	if (PlayerCameraManager)
	{
		BodyCameraModifier = Cast<UTHCameraModifier>(PlayerCameraManager->AddNewCameraModifier(UTHCameraModifier::StaticClass()));
	}
}

float ATHPlayerController::GetLookScale() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UTHControlsSettingsSubsystem* Controls = GameInstance ? GameInstance->GetSubsystem<UTHControlsSettingsSubsystem>() : nullptr;
	if (!Controls)
	{
		return 1.0f;
	}
	float AimAlpha = 0.0f;
	if (const APawn* MyPawn = GetPawn())
	{
		if (const UTHWeaponComponent* Weapons = MyPawn->FindComponentByClass<UTHWeaponComponent>())
		{
			AimAlpha = Weapons->GetAimAlpha();
		}
	}
	return Controls->GetLookSensitivity() * FMath::Lerp(1.0f, Controls->GetAimSensitivity(), AimAlpha);
}

void ATHPlayerController::AddYawInput(float Val)
{
	Super::AddYawInput(Val * GetLookScale());
}

void ATHPlayerController::AddPitchInput(float Val)
{
	Super::AddPitchInput(Val * GetLookScale());
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

void ATHPlayerController::GiveFlashlight()
{
	if (UTHWeaponComponent* Weapons = GetPawn() ? GetPawn()->FindComponentByClass<UTHWeaponComponent>() : nullptr)
	{
		Weapons->GiveHandFlashlight(true);
	}
}

void ATHPlayerController::WeaponLight()
{
	if (UTHWeaponComponent* Weapons = GetPawn() ? GetPawn()->FindComponentByClass<UTHWeaponComponent>() : nullptr)
	{
		Weapons->ToggleWeaponLight();
	}
}

void ATHPlayerController::Sanity(float Value)
{
	ServerSetSanity(Value);
}

void ATHPlayerController::ServerSetSanity_Implementation(float Value)
{
	if (UTHSanityComponent* Mind = GetPawn() ? GetPawn()->FindComponentByClass<UTHSanityComponent>() : nullptr)
	{
		Mind->SetSanity(Value);
	}
}

void ATHPlayerController::Hallucinate(const FString& Name)
{
	if (UTHSanityComponent* Mind = GetPawn() ? GetPawn()->FindComponentByClass<UTHSanityComponent>() : nullptr)
	{
		if (Name.Equals(TEXT("FakeEnemy"), ESearchCase::IgnoreCase))
		{
			Mind->SendFakeEnemy();
		}
		else
		{
			Mind->PlayHallucination(Name.IsEmpty() ? NAME_None : FName(*Name));
		}
	}
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
	if (StatusWidget)
	{
		StatusWidget->RemoveFromParent();
		StatusWidget = nullptr;
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

void ATHPlayerController::NotifyLocalHit(float Damage)
{
	// About 40 damage (an enemy's blow) is a full-strength hit
	const float Strength = FMath::Clamp(Damage / 40.0f, 0.25f, 1.5f);
	if (BodyCameraModifier)
	{
		BodyCameraModifier->AddHit(Strength);
	}
	if (StatusWidget)
	{
		StatusWidget->ShowHit(Strength);
	}
}

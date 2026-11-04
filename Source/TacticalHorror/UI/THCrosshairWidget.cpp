#include "UI/THCrosshairWidget.h"
#include "Settings/THCrosshairSettingsSubsystem.h"
#include "Weapons/THWeaponComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

void UTHCrosshairWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// No Widget Blueprint layout: a centred image on a transparent full-screen root
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
		Root->SetBrushColor(FLinearColor::Transparent);
		Root->SetHorizontalAlignment(HAlign_Center);
		Root->SetVerticalAlignment(VAlign_Center);
		Root->SetPadding(FMargin(0.0f));
		WidgetTree->RootWidget = Root;

		CrosshairImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CrosshairImage"));
		Root->SetContent(CrosshairImage);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTHCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UTHCrosshairSettingsSubsystem* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTHCrosshairSettingsSubsystem>() : nullptr)
	{
		ChangedHandle = Settings->OnChanged.AddUObject(this, &UTHCrosshairWidget::ApplySettings);
	}
	ApplySettings();
}

void UTHCrosshairWidget::NativeDestruct()
{
	if (UTHCrosshairSettingsSubsystem* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTHCrosshairSettingsSubsystem>() : nullptr)
	{
		Settings->OnChanged.Remove(ChangedHandle);
	}
	Super::NativeDestruct();
}

void UTHCrosshairWidget::ApplySettings()
{
	const UTHCrosshairSettingsSubsystem* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTHCrosshairSettingsSubsystem>() : nullptr;
	if (!Settings || !CrosshairImage)
	{
		return;
	}
	UTexture2D* Texture = Settings->GetStyleTexture(Settings->GetStyle());
	CrosshairImage->SetBrushFromTexture(Texture, false);
	CrosshairImage->SetDesiredSizeOverride(FVector2D(Settings->GetSize()));
	CrosshairImage->SetColorAndOpacity(FLinearColor(Settings->GetColor()));
}

bool UTHCrosshairWidget::ShouldShow() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || PC->GetViewTarget() != Pawn)
	{
		return false;
	}
	const UTHWeaponComponent* Weapons = Pawn->FindComponentByClass<UTHWeaponComponent>();
	return Weapons && Weapons->GetCurrentWeapon() && !Weapons->IsAiming();
}

void UTHCrosshairWidget::FindTemplateCrosshair()
{
	const APlayerController* PC = GetOwningPlayer();
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!Widget->GetClass()->GetName().StartsWith(TEXT("WB_Crosshair")) || Widget->GetOwningPlayer() != PC || !Widget->WidgetTree)
		{
			continue;
		}
		Widget->WidgetTree->ForEachWidget([this](UWidget* Child)
		{
			if (UImage* Image = Cast<UImage>(Child))
			{
				const UObject* Resource = Image->GetBrush().GetResourceObject();
				const FString Name = Resource ? Resource->GetName() : FString();
				if (Name.Contains(TEXT("Crosshair_Dot")))
				{
					TemplateDot = Image;
				}
				else if (Name.Contains(TEXT("Cursor")))
				{
					TemplateHand = Image;
				}
			}
		});
		return;
	}
}

bool UTHCrosshairWidget::IsTemplateHandShowing() const
{
	const UImage* Hand = TemplateHand.Get();
	return Hand && Hand->IsVisible() && Hand->GetRenderOpacity() > 0.5f;
}

void UTHCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The template HUD is created by the character, possibly after this widget
	if (!TemplateDot.IsValid())
	{
		TemplateSearchCooldown -= InDeltaTime;
		if (TemplateSearchCooldown <= 0.0f)
		{
			TemplateSearchCooldown = 1.0f;
			FindTemplateCrosshair();
		}
	}
	if (UImage* Dot = TemplateDot.Get(); Dot && Dot->GetVisibility() != ESlateVisibility::Hidden)
	{
		Dot->SetVisibility(ESlateVisibility::Hidden);
	}

	if (CrosshairImage)
	{
		const ESlateVisibility Wanted = ShouldShow() && !IsTemplateHandShowing() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
		if (CrosshairImage->GetVisibility() != Wanted)
		{
			CrosshairImage->SetVisibility(Wanted);
		}
	}
}

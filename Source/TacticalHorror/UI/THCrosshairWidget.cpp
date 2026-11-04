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
	// Every template crosshair in this world (the HUD can be made more than once, e.g. on each possession), so no
	// copy of the dot is missed. Matched by world rather than owning player: in multi-player PIE each instance has its own world.
	TemplateDots.Reset();
	TemplateHands.Reset();
	const UWorld* World = GetWorld();
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->GetWorld() != World || !Widget->WidgetTree || !Widget->GetClass()->GetName().StartsWith(TEXT("WB_Crosshair")))
		{
			continue;
		}
		Widget->WidgetTree->ForEachWidget([this](UWidget* Child)
		{
			if (UImage* Image = Cast<UImage>(Child))
			{
				const UObject* Resource = Image->GetBrush().GetResourceObject();
				const FString Name = Resource ? Resource->GetName() : FString();
				if (Name.Contains(TEXT("Dot")))
				{
					TemplateDots.Add(Image);
				}
				else if (Name.Contains(TEXT("Cursor")))
				{
					TemplateHands.Add(Image);
				}
			}
		});
	}
}

bool UTHCrosshairWidget::IsTemplateHandShowing() const
{
	for (const TWeakObjectPtr<UImage>& Weak : TemplateHands)
	{
		const UImage* Hand = Weak.Get();
		if (Hand && Hand->IsVisible() && Hand->GetRenderOpacity() > 0.5f && Hand->GetCachedWidget().IsValid())
		{
			return true;
		}
	}
	return false;
}

void UTHCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The template HUD is created by the character, possibly after this widget and possibly again later: look every second
	TemplateSearchCooldown -= InDeltaTime;
	if (TemplateSearchCooldown <= 0.0f)
	{
		TemplateSearchCooldown = 1.0f;
		FindTemplateCrosshair();
	}
	// Opacity as well as visibility, in case the template's own logic shows the dot again
	for (const TWeakObjectPtr<UImage>& Weak : TemplateDots)
	{
		if (UImage* Dot = Weak.Get())
		{
			if (Dot->GetVisibility() != ESlateVisibility::Hidden)
			{
				Dot->SetVisibility(ESlateVisibility::Hidden);
			}
			if (Dot->GetRenderOpacity() != 0.0f)
			{
				Dot->SetRenderOpacity(0.0f);
			}
		}
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

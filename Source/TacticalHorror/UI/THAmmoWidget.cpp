#include "UI/THAmmoWidget.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Weapons/THWeaponData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "THAmmo"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		return Block;
	}
}

void UTHAmmoWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTHAmmoWidget::BuildLayout()
{
	// Full-screen transparent root; the readout sits in the bottom-right corner
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(FLinearColor::Transparent);
	Root->SetHorizontalAlignment(HAlign_Right);
	Root->SetVerticalAlignment(VAlign_Bottom);
	Root->SetPadding(FMargin(0.0f, 0.0f, 48.0f, 40.0f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Box);

	// "24 / 90": big magazine count, smaller reserve
	UHorizontalBox* Counts = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(Counts)->SetHorizontalAlignment(HAlign_Right);

	MagazineText = MakeText(WidgetTree, TEXT("MagazineText"), 34);
	Counts->AddChildToHorizontalBox(MagazineText)->SetVerticalAlignment(VAlign_Bottom);

	ReserveText = MakeText(WidgetTree, TEXT("ReserveText"), 18);
	ReserveText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
	UHorizontalBoxSlot* ReserveSlot = Counts->AddChildToHorizontalBox(ReserveText);
	ReserveSlot->SetVerticalAlignment(VAlign_Bottom);
	ReserveSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 5.0f));

	FireModeText = MakeText(WidgetTree, TEXT("FireModeText"), 12);
	FireModeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
	Box->AddChildToVerticalBox(FireModeText)->SetHorizontalAlignment(HAlign_Right);
}

ATHWeaponBase* UTHAmmoWidget::GetShownWeapon() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || PC->GetViewTarget() != Pawn)
	{
		return nullptr;
	}
	const UTHWeaponComponent* Weapons = Pawn->FindComponentByClass<UTHWeaponComponent>();
	return Weapons ? Weapons->GetCurrentWeapon() : nullptr;
}

void UTHAmmoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ATHWeaponBase* Weapon = GetShownWeapon();
	const UTHWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;

	// Hide the content, not this widget: a hidden widget isn't ticked, so it would never come back
	if (UWidget* Content = WidgetTree ? WidgetTree->RootWidget : nullptr)
	{
		const ESlateVisibility Wanted = Data ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
		if (Content->GetVisibility() != Wanted)
		{
			Content->SetVisibility(Wanted);
		}
	}
	if (!Data)
	{
		ShownWeapon = nullptr;
		return;
	}

	const int32 Magazine = Weapon->GetCurrentAmmo();
	const int32 Reserve = Weapon->GetReserveAmmo();
	const bool bCanSwitch = Data->FireModes.Num() > 1;
	const int32 FireMode = bCanSwitch ? static_cast<int32>(Weapon->GetFireMode()) : -1;
	const bool bReloading = Weapon->IsReloading();
	if (Weapon == ShownWeapon.Get() && Magazine == ShownMagazine && Reserve == ShownReserve && FireMode == ShownFireMode && bReloading == bShownReloading)
	{
		return;
	}
	ShownWeapon = Weapon;
	ShownMagazine = Magazine;
	ShownReserve = Reserve;
	ShownFireMode = FireMode;
	bShownReloading = bReloading;

	if (MagazineText)
	{
		MagazineText->SetText(FText::AsNumber(Magazine));
		const bool bLow = Magazine <= FMath::FloorToInt(Data->MagazineSize * LowAmmoFraction);
		MagazineText->SetColorAndOpacity(FSlateColor(bLow ? LowAmmoColor : NormalColor));
	}
	if (ReserveText)
	{
		ReserveText->SetText(FText::Format(LOCTEXT("Reserve", "/ {0}"), FText::AsNumber(Reserve)));
	}
	if (FireModeText)
	{
		FText Mode = FText::GetEmpty();
		if (bReloading)
		{
			Mode = LOCTEXT("Reloading", "RELOADING");
		}
		else if (bCanSwitch)
		{
			Mode = Weapon->GetFireMode() == ETHFireMode::FullAuto ? LOCTEXT("Auto", "AUTO") : LOCTEXT("Semi", "SEMI");
		}
		FireModeText->SetText(Mode);
	}
	ReceiveAmmoChanged(Magazine, Reserve, Data->MagazineSize, bReloading);
}

#undef LOCTEXT_NAMESPACE

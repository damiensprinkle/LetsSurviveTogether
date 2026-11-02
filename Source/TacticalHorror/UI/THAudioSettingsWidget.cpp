#include "UI/THAudioSettingsWidget.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "THAudioSettings"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		return Block;
	}
}

TSharedRef<SWidget> UTHAudioSettingsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UTHAudioSettingsWidget::BuildLayout()
{
	// Dimmed full-screen backdrop with a centred panel
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Backdrop->SetHorizontalAlignment(HAlign_Center);
	Backdrop->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Backdrop;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.95f));
	Panel->SetPadding(FMargin(32.0f, 24.0f));
	Backdrop->SetContent(Panel);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(460.0f);
	Panel->SetContent(Size);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->SetContent(Box);

	UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Title", "Audio"), 24));
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	MasterSlider = AddVolumeRow(Box, LOCTEXT("Master", "Master"), MasterValueText);
	MasterSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleMasterChanged);

	GunshotSlider = AddVolumeRow(Box, LOCTEXT("Gunshots", "Gunshots"), GunshotValueText);
	GunshotSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleGunshotChanged);

	UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseText = MakeText(WidgetTree, LOCTEXT("Close", "Close"), 14);
	CloseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
	Close->SetContent(CloseText);
	Close->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandleCloseClicked);
	UVerticalBoxSlot* CloseSlot = Box->AddChildToVerticalBox(Close);
	CloseSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
	CloseSlot->SetHorizontalAlignment(HAlign_Right);
}

USlider* UTHAudioSettingsWidget::AddVolumeRow(UVerticalBox* Box, const FText& Label, TObjectPtr<UTextBlock>& OutValueText)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.0f, 6.0f));

	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeText(WidgetTree, Label, 16));
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	USlider* Slider = WidgetTree->ConstructWidget<USlider>();
	Slider->SetMinValue(0.0f);
	Slider->SetMaxValue(1.0f);
	Slider->SetStepSize(0.01f);
	UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
	FSlateChildSize SliderSize(ESlateSizeRule::Fill);
	SliderSize.Value = 1.6f;
	SliderSlot->SetSize(SliderSize);
	SliderSlot->SetVerticalAlignment(VAlign_Center);

	USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
	ValueBox->SetWidthOverride(56.0f);
	OutValueText = MakeText(WidgetTree, FText::GetEmpty(), 16);
	OutValueText->SetJustification(ETextJustify::Right);
	ValueBox->SetContent(OutValueText);
	UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueBox);
	ValueSlot->SetVerticalAlignment(VAlign_Center);

	return Slider;
}

void UTHAudioSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

void UTHAudioSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (const UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		if (MasterSlider)
		{
			MasterSlider->SetValue(Settings->GetMasterVolume());
			MasterValueText->SetText(FormatPercent(Settings->GetMasterVolume()));
		}
		if (GunshotSlider)
		{
			GunshotSlider->SetValue(Settings->GetGunshotVolume());
			GunshotValueText->SetText(FormatPercent(Settings->GetGunshotVolume()));
		}
	}
}

FReply UTHAudioSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::P)
	{
		OnCloseRequested.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UTHAudioSettingsSubsystem* UTHAudioSettingsWidget::GetSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTHAudioSettingsSubsystem>() : nullptr;
}

FText UTHAudioSettingsWidget::FormatPercent(float Value)
{
	return FText::AsPercent(Value);
}

void UTHAudioSettingsWidget::HandleMasterChanged(float Value)
{
	if (UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		Settings->SetMasterVolume(Value);
	}
	MasterValueText->SetText(FormatPercent(Value));
}

void UTHAudioSettingsWidget::HandleGunshotChanged(float Value)
{
	if (UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		Settings->SetGunshotVolume(Value);
	}
	GunshotValueText->SetText(FormatPercent(Value));
}

void UTHAudioSettingsWidget::HandleCloseClicked()
{
	OnCloseRequested.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE

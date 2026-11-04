#include "UI/THAudioSettingsWidget.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Settings/THCrosshairSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"

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

	UButton* MakeButton(UWidgetTree* Tree, const FText& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		UTextBlock* Text = MakeText(Tree, Label, 14);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
		Button->SetContent(Text);
		return Button;
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
	Size->SetWidthOverride(520.0f);
	Panel->SetContent(Size);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->SetContent(Box);

	UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Title", "Settings"), 24));
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// Audio
	AddSectionTitle(Box, LOCTEXT("AudioSection", "Audio"));
	MasterSlider = AddVolumeRow(Box, LOCTEXT("Master", "Master"), MasterValueText);
	MasterSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleMasterChanged);
	GunshotSlider = AddVolumeRow(Box, LOCTEXT("Gunshots", "Gunshots"), GunshotValueText);
	GunshotSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleGunshotChanged);

	// Crosshair
	AddSectionTitle(Box, LOCTEXT("CrosshairSection", "Crosshair"));
	AddCrosshairStyleRow(Box);
	SizeSlider = AddSliderRow(Box, LOCTEXT("CrosshairSize", "Size"),
		UTHCrosshairSettingsSubsystem::MinSize, UTHCrosshairSettingsSubsystem::MaxSize, 1.0f, SizeValueText);
	SizeSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleSizeChanged);
	ColorSliders[0] = AddSliderRow(Box, LOCTEXT("Red", "Red"), 0.0f, 255.0f, 1.0f, ColorValueTexts[0]);
	ColorSliders[0]->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleRedChanged);
	ColorSliders[1] = AddSliderRow(Box, LOCTEXT("Green", "Green"), 0.0f, 255.0f, 1.0f, ColorValueTexts[1]);
	ColorSliders[1]->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleGreenChanged);
	ColorSliders[2] = AddSliderRow(Box, LOCTEXT("Blue", "Blue"), 0.0f, 255.0f, 1.0f, ColorValueTexts[2]);
	ColorSliders[2]->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleBlueChanged);

	// Bottom row: Leave Game on the left (in game only), Close on the right
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* ButtonsSlot = Box->AddChildToVerticalBox(Buttons);
	ButtonsSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
	if (bShowLeaveGame)
	{
		UButton* Leave = MakeButton(WidgetTree, LOCTEXT("LeaveGame", "Leave Game"));
		Leave->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandleLeaveGameClicked);
		Buttons->AddChildToHorizontalBox(Leave);
	}
	UHorizontalBoxSlot* SpacerSlot = Buttons->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USizeBox>());
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* Close = MakeButton(WidgetTree, LOCTEXT("Close", "Close"));
	Close->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandleCloseClicked);
	Buttons->AddChildToHorizontalBox(Close);
}

void UTHAudioSettingsWidget::AddSectionTitle(UVerticalBox* Box, const FText& Title)
{
	UTextBlock* Text = MakeText(WidgetTree, Title, 15);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
	UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(Text);
	TitleSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 4.0f));
}

USlider* UTHAudioSettingsWidget::AddSliderRow(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, TObjectPtr<UTextBlock>& OutValueText)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.0f, 6.0f));

	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeText(WidgetTree, Label, 16));
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	USlider* Slider = WidgetTree->ConstructWidget<USlider>();
	Slider->SetMinValue(Min);
	Slider->SetMaxValue(Max);
	Slider->SetStepSize(Step);
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

USlider* UTHAudioSettingsWidget::AddVolumeRow(UVerticalBox* Box, const FText& Label, TObjectPtr<UTextBlock>& OutValueText)
{
	return AddSliderRow(Box, Label, 0.0f, 1.0f, 0.01f, OutValueText);
}

void UTHAudioSettingsWidget::AddCrosshairStyleRow(UVerticalBox* Box)
{
	// Label · < · preview · > · "n / N"
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.0f, 6.0f));

	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeText(WidgetTree, LOCTEXT("Style", "Style"), 16));
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	UButton* Previous = MakeButton(WidgetTree, LOCTEXT("PreviousStyle", "<"));
	Previous->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandlePreviousStyle);
	Row->AddChildToHorizontalBox(Previous)->SetVerticalAlignment(VAlign_Center);

	// Preview on a mid-grey square so dark and light colours both show
	UBorder* PreviewBox = WidgetTree->ConstructWidget<UBorder>();
	PreviewBox->SetBrushColor(FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
	PreviewBox->SetHorizontalAlignment(HAlign_Center);
	PreviewBox->SetVerticalAlignment(VAlign_Center);
	USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>();
	PreviewSize->SetWidthOverride(UTHCrosshairSettingsSubsystem::MaxSize + 8.0f);
	PreviewSize->SetHeightOverride(UTHCrosshairSettingsSubsystem::MaxSize + 8.0f);
	PreviewSize->SetContent(PreviewBox);
	CrosshairPreview = WidgetTree->ConstructWidget<UImage>();
	PreviewBox->SetContent(CrosshairPreview);
	UHorizontalBoxSlot* PreviewSlot = Row->AddChildToHorizontalBox(PreviewSize);
	PreviewSlot->SetPadding(FMargin(10.0f, 0.0f));
	PreviewSlot->SetVerticalAlignment(VAlign_Center);

	UButton* Next = MakeButton(WidgetTree, LOCTEXT("NextStyle", ">"));
	Next->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandleNextStyle);
	Row->AddChildToHorizontalBox(Next)->SetVerticalAlignment(VAlign_Center);

	USizeBox* CountBox = WidgetTree->ConstructWidget<USizeBox>();
	CountBox->SetWidthOverride(56.0f);
	StyleText = MakeText(WidgetTree, FText::GetEmpty(), 16);
	StyleText->SetJustification(ETextJustify::Right);
	CountBox->SetContent(StyleText);
	Row->AddChildToHorizontalBox(CountBox)->SetVerticalAlignment(VAlign_Center);
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
	RefreshCrosshairControls();
}

void UTHAudioSettingsWidget::NativeDestruct()
{
	if (UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings())
	{
		Crosshair->SaveSettings();
	}
	Super::NativeDestruct();
}

void UTHAudioSettingsWidget::RefreshCrosshairControls()
{
	const UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings();
	if (!Crosshair)
	{
		return;
	}
	const FColor Color = Crosshair->GetColor();
	if (CrosshairPreview)
	{
		CrosshairPreview->SetBrushFromTexture(Crosshair->GetStyleTexture(Crosshair->GetStyle()), false);
		CrosshairPreview->SetDesiredSizeOverride(FVector2D(Crosshair->GetSize()));
		CrosshairPreview->SetColorAndOpacity(FLinearColor(Color));
	}
	if (StyleText)
	{
		StyleText->SetText(FText::Format(LOCTEXT("StyleCount", "{0} / {1}"),
			FText::AsNumber(Crosshair->GetStyle() + 1), FText::AsNumber(UTHCrosshairSettingsSubsystem::GetStyles().Num())));
	}
	if (SizeSlider)
	{
		SizeSlider->SetValue(Crosshair->GetSize());
		SizeValueText->SetText(FText::AsNumber(FMath::RoundToInt(Crosshair->GetSize())));
	}
	const uint8 Channels[3] = { Color.R, Color.G, Color.B };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (ColorSliders[Index])
		{
			ColorSliders[Index]->SetValue(Channels[Index]);
			ColorValueTexts[Index]->SetText(FText::AsNumber(Channels[Index]));
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

UTHCrosshairSettingsSubsystem* UTHAudioSettingsWidget::GetCrosshairSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTHCrosshairSettingsSubsystem>() : nullptr;
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

void UTHAudioSettingsWidget::HandlePreviousStyle()
{
	if (UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings())
	{
		Crosshair->SetStyle(Crosshair->GetStyle() - 1);
	}
	RefreshCrosshairControls();
}

void UTHAudioSettingsWidget::HandleNextStyle()
{
	if (UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings())
	{
		Crosshair->SetStyle(Crosshair->GetStyle() + 1);
	}
	RefreshCrosshairControls();
}

void UTHAudioSettingsWidget::HandleSizeChanged(float Value)
{
	if (UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings())
	{
		Crosshair->SetSize(Value);
	}
	RefreshCrosshairControls();
}

void UTHAudioSettingsWidget::SetColorChannel(int32 Channel, float Value)
{
	if (UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings())
	{
		FColor Color = Crosshair->GetColor();
		const uint8 Byte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value), 0, 255));
		(Channel == 0 ? Color.R : Channel == 1 ? Color.G : Color.B) = Byte;
		Crosshair->SetColor(Color);
	}
	RefreshCrosshairControls();
}

void UTHAudioSettingsWidget::HandleRedChanged(float Value)
{
	SetColorChannel(0, Value);
}

void UTHAudioSettingsWidget::HandleGreenChanged(float Value)
{
	SetColorChannel(1, Value);
}

void UTHAudioSettingsWidget::HandleBlueChanged(float Value)
{
	SetColorChannel(2, Value);
}

void UTHAudioSettingsWidget::HandleCloseClicked()
{
	OnCloseRequested.ExecuteIfBound();
}

void UTHAudioSettingsWidget::HandleLeaveGameClicked()
{
	OnLeaveGameRequested.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE

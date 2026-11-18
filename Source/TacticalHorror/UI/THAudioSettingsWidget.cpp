#include "UI/THAudioSettingsWidget.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Settings/THCrosshairSettingsSubsystem.h"
#include "Settings/THControlsSettingsSubsystem.h"
#include "Settings/THDisplaySettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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

	// Scrolls if the screen is too short for every section
	Size->SetMaxDesiredHeight(820.0f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Size->SetContent(Scroll);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Box);

	UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Title", "Settings"), 24));
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// Controls
	AddSectionTitle(Box, LOCTEXT("ControlsSection", "Controls"));
	LookSensitivitySlider = AddSliderRow(Box, LOCTEXT("LookSensitivity", "Look Sensitivity"),
		UTHControlsSettingsSubsystem::MinSensitivity, UTHControlsSettingsSubsystem::MaxSensitivity, 0.005f, LookSensitivityValueText);
	LookSensitivitySlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleLookSensitivityChanged);
	AimSensitivitySlider = AddSliderRow(Box, LOCTEXT("AimSensitivity", "Aim Sensitivity"),
		UTHControlsSettingsSubsystem::MinSensitivity, UTHControlsSettingsSubsystem::MaxSensitivity, 0.005f, AimSensitivityValueText);
	AimSensitivitySlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleAimSensitivityChanged);
	AddSensitivityConverterRows(Box);

	// Audio
	AddSectionTitle(Box, LOCTEXT("AudioSection", "Audio"));
	MasterSlider = AddVolumeRow(Box, LOCTEXT("Master", "Master"), MasterValueText);
	MasterSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleMasterChanged);
	GunplaySlider = AddVolumeRow(Box, LOCTEXT("Gunplay", "Gunplay"), GunplayValueText);
	GunplaySlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleGunplayChanged);
	MusicSlider = AddVolumeRow(Box, LOCTEXT("Music", "Music & ambience"), MusicValueText);
	MusicSlider->OnValueChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleMusicChanged);
	SpatialAudioCheck = AddToggleRow(Box, LOCTEXT("SpatialAudio", "3D Audio (headphones)"));
	SpatialAudioCheck->OnCheckStateChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleSpatialAudioChanged);

	// Display
	AddSectionTitle(Box, LOCTEXT("DisplaySection", "Display"));
	BloodCheck = AddToggleRow(Box, LOCTEXT("Blood", "Blood"));
	BloodCheck->OnCheckStateChanged.AddDynamic(this, &UTHAudioSettingsWidget::HandleBloodChanged);

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

UCheckBox* UTHAudioSettingsWidget::AddToggleRow(UVerticalBox* Box, const FText& Label)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 6.0f));
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeText(WidgetTree, Label, 16));
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	UCheckBox* Check = WidgetTree->ConstructWidget<UCheckBox>();
	Row->AddChildToHorizontalBox(Check)->SetVerticalAlignment(VAlign_Center);
	return Check;
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

void UTHAudioSettingsWidget::AddSensitivityConverterRows(UVerticalBox* Box)
{
	// Convert from · < · game · >
	UHorizontalBox* GameRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(GameRow)->SetPadding(FMargin(0.0f, 6.0f));
	UHorizontalBoxSlot* GameLabelSlot = GameRow->AddChildToHorizontalBox(MakeText(WidgetTree, LOCTEXT("ConvertFrom", "Convert From"), 16));
	GameLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GameLabelSlot->SetVerticalAlignment(VAlign_Center);

	UButton* Previous = MakeButton(WidgetTree, LOCTEXT("PreviousGame", "<"));
	Previous->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandlePreviousConverterGame);
	GameRow->AddChildToHorizontalBox(Previous)->SetVerticalAlignment(VAlign_Center);

	USizeBox* GameBox = WidgetTree->ConstructWidget<USizeBox>();
	GameBox->SetWidthOverride(250.0f);
	ConverterGameText = MakeText(WidgetTree, FText::GetEmpty(), 14);
	ConverterGameText->SetJustification(ETextJustify::Center);
	GameBox->SetContent(ConverterGameText);
	UHorizontalBoxSlot* GameNameSlot = GameRow->AddChildToHorizontalBox(GameBox);
	GameNameSlot->SetPadding(FMargin(6.0f, 0.0f));
	GameNameSlot->SetVerticalAlignment(VAlign_Center);

	UButton* Next = MakeButton(WidgetTree, LOCTEXT("NextGame", ">"));
	Next->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandleNextConverterGame);
	GameRow->AddChildToHorizontalBox(Next)->SetVerticalAlignment(VAlign_Center);

	// Their sensitivity · [value] · Convert
	UHorizontalBox* ValueRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(ValueRow)->SetPadding(FMargin(0.0f, 6.0f));
	UHorizontalBoxSlot* ValueLabelSlot = ValueRow->AddChildToHorizontalBox(MakeText(WidgetTree, LOCTEXT("TheirSensitivity", "Their Sensitivity"), 16));
	ValueLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ValueLabelSlot->SetVerticalAlignment(VAlign_Center);

	USizeBox* InputBox = WidgetTree->ConstructWidget<USizeBox>();
	InputBox->SetWidthOverride(110.0f);
	TheirSensitivityBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	TheirSensitivityBox->SetHintText(LOCTEXT("TheirSensitivityHint", "e.g. 1.25"));
	TheirSensitivityBox->OnTextCommitted.AddDynamic(this, &UTHAudioSettingsWidget::HandleTheirSensitivityCommitted);
	InputBox->SetContent(TheirSensitivityBox);
	UHorizontalBoxSlot* InputSlot = ValueRow->AddChildToHorizontalBox(InputBox);
	InputSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	InputSlot->SetVerticalAlignment(VAlign_Center);

	UButton* Convert = MakeButton(WidgetTree, LOCTEXT("Convert", "Convert"));
	Convert->OnClicked.AddDynamic(this, &UTHAudioSettingsWidget::HandleConvertClicked);
	ValueRow->AddChildToHorizontalBox(Convert)->SetVerticalAlignment(VAlign_Center);

	// What happened, under the rows
	ConverterResultText = MakeText(WidgetTree, FText::GetEmpty(), 13);
	ConverterResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
	ConverterResultText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(ConverterResultText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
}

void UTHAudioSettingsWidget::RefreshConverterGame()
{
	const TArray<FTHOtherGameSensitivity>& Games = UTHControlsSettingsSubsystem::GetOtherGames();
	if (ConverterGameText && Games.IsValidIndex(ConverterGame))
	{
		ConverterGameText->SetText(Games[ConverterGame].Name);
	}
}

void UTHAudioSettingsWidget::RefreshSensitivityControls()
{
	const UTHControlsSettingsSubsystem* Controls = GetControlsSettings();
	if (!Controls)
	{
		return;
	}
	if (LookSensitivitySlider)
	{
		LookSensitivitySlider->SetValue(Controls->GetLookSensitivity());
		LookSensitivityValueText->SetText(FormatSensitivity(Controls->GetLookSensitivity()));
	}
	if (AimSensitivitySlider)
	{
		AimSensitivitySlider->SetValue(Controls->GetAimSensitivity());
		AimSensitivityValueText->SetText(FormatSensitivity(Controls->GetAimSensitivity()));
	}
}

void UTHAudioSettingsWidget::HandlePreviousConverterGame()
{
	const int32 Count = UTHControlsSettingsSubsystem::GetOtherGames().Num();
	ConverterGame = Count > 0 ? (ConverterGame + Count - 1) % Count : 0;
	RefreshConverterGame();
}

void UTHAudioSettingsWidget::HandleNextConverterGame()
{
	const int32 Count = UTHControlsSettingsSubsystem::GetOtherGames().Num();
	ConverterGame = Count > 0 ? (ConverterGame + 1) % Count : 0;
	RefreshConverterGame();
}

void UTHAudioSettingsWidget::HandleTheirSensitivityCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleConvertClicked();
	}
}

void UTHAudioSettingsWidget::HandleConvertClicked()
{
	UTHControlsSettingsSubsystem* Controls = GetControlsSettings();
	if (!Controls || !TheirSensitivityBox || !ConverterResultText)
	{
		return;
	}
	// Accept a comma as the decimal point too
	const FString Entered = TheirSensitivityBox->GetText().ToString().TrimStartAndEnd().Replace(TEXT(","), TEXT("."));
	const float Theirs = Entered.IsNumeric() ? FCString::Atof(*Entered) : 0.0f;
	if (Theirs <= 0.0f)
	{
		ConverterResultText->SetText(LOCTEXT("ConvertBadNumber", "Enter your sensitivity from that game, e.g. 1.25."));
		return;
	}
	const float Ours = UTHControlsSettingsSubsystem::ConvertFrom(ConverterGame, Theirs);
	Controls->SetLookSensitivity(Ours);
	RefreshSensitivityControls();
	const FText GameName = UTHControlsSettingsSubsystem::GetOtherGames()[ConverterGame].Name;
	if (Ours < UTHControlsSettingsSubsystem::MinSensitivity || Ours > UTHControlsSettingsSubsystem::MaxSensitivity)
	{
		ConverterResultText->SetText(FText::Format(LOCTEXT("ConvertOutOfRange", "{0} in {1} is {2} here, outside the range, so Look Sensitivity is set to {3}."),
			FText::AsNumber(Theirs), GameName, FormatSensitivity(Ours), FormatSensitivity(Controls->GetLookSensitivity())));
	}
	else
	{
		ConverterResultText->SetText(FText::Format(LOCTEXT("ConvertDone", "{0} in {1}: Look Sensitivity set to {2}. Uses the same mouse DPI; aim sensitivity is not converted."),
			FText::AsNumber(Theirs), GameName, FormatSensitivity(Ours)));
	}
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
		if (GunplaySlider)
		{
			GunplaySlider->SetValue(Settings->GetGunplayVolume());
			GunplayValueText->SetText(FormatPercent(Settings->GetGunplayVolume()));
		}
		if (MusicSlider)
		{
			MusicSlider->SetValue(Settings->GetMusicVolume());
			MusicValueText->SetText(FormatPercent(Settings->GetMusicVolume()));
		}
		if (SpatialAudioCheck)
		{
			SpatialAudioCheck->SetIsChecked(Settings->Is3DAudioEnabled());
		}
	}
	if (const UTHDisplaySettingsSubsystem* Display = GetDisplaySettings(); Display && BloodCheck)
	{
		BloodCheck->SetIsChecked(Display->IsBloodEnabled());
	}
	RefreshSensitivityControls();
	RefreshConverterGame();
	RefreshCrosshairControls();
}

void UTHAudioSettingsWidget::NativeDestruct()
{
	if (UTHControlsSettingsSubsystem* Controls = GetControlsSettings())
	{
		Controls->SaveSettings();
	}
	if (UTHCrosshairSettingsSubsystem* Crosshair = GetCrosshairSettings())
	{
		Crosshair->SaveSettings();
	}
	if (UTHDisplaySettingsSubsystem* Display = GetDisplaySettings())
	{
		Display->SaveSettings();
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

UTHControlsSettingsSubsystem* UTHAudioSettingsWidget::GetControlsSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTHControlsSettingsSubsystem>() : nullptr;
}

UTHDisplaySettingsSubsystem* UTHAudioSettingsWidget::GetDisplaySettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTHDisplaySettingsSubsystem>() : nullptr;
}

void UTHAudioSettingsWidget::HandleBloodChanged(bool bIsChecked)
{
	if (UTHDisplaySettingsSubsystem* Display = GetDisplaySettings())
	{
		Display->SetBloodEnabled(bIsChecked);
	}
}

FText UTHAudioSettingsWidget::FormatSensitivity(float Value)
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 3;
	Options.MaximumFractionalDigits = 3;
	return FText::AsNumber(Value, &Options);
}

FText UTHAudioSettingsWidget::FormatPercent(float Value)
{
	return FText::AsPercent(Value);
}

void UTHAudioSettingsWidget::HandleLookSensitivityChanged(float Value)
{
	if (UTHControlsSettingsSubsystem* Controls = GetControlsSettings())
	{
		Controls->SetLookSensitivity(Value);
	}
	LookSensitivityValueText->SetText(FormatSensitivity(Value));
}

void UTHAudioSettingsWidget::HandleAimSensitivityChanged(float Value)
{
	if (UTHControlsSettingsSubsystem* Controls = GetControlsSettings())
	{
		Controls->SetAimSensitivity(Value);
	}
	AimSensitivityValueText->SetText(FormatSensitivity(Value));
}

void UTHAudioSettingsWidget::HandleMasterChanged(float Value)
{
	if (UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		Settings->SetMasterVolume(Value);
	}
	MasterValueText->SetText(FormatPercent(Value));
}

void UTHAudioSettingsWidget::HandleGunplayChanged(float Value)
{
	if (UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		Settings->SetGunplayVolume(Value);
	}
	GunplayValueText->SetText(FormatPercent(Value));
}

void UTHAudioSettingsWidget::HandleMusicChanged(float Value)
{
	if (UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		Settings->SetMusicVolume(Value);
	}
	MusicValueText->SetText(FormatPercent(Value));
}

void UTHAudioSettingsWidget::HandleSpatialAudioChanged(bool bIsChecked)
{
	if (UTHAudioSettingsSubsystem* Settings = GetSettings())
	{
		Settings->Set3DAudioEnabled(bIsChecked);
	}
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

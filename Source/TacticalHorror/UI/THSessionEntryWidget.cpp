#include "UI/THSessionEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#define LOCTEXT_NAMESPACE "THMainMenu"

void UTHSessionEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// No Widget Blueprint layout: build the plain one
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UTHSessionEntryWidget::HandleJoinClicked);
	}
}

void UTHSessionEntryWidget::BuildLayout()
{
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Row"));
	Row->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
	Row->SetPadding(FMargin(12.0f, 8.0f));
	WidgetTree->RootWidget = Row;

	UHorizontalBox* Box = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->SetContent(Box);

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* TextsSlot = Box->AddChildToHorizontalBox(Texts);
	TextsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextsSlot->SetVerticalAlignment(VAlign_Center);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 16;
	NameText->SetFont(NameFont);
	Texts->AddChildToVerticalBox(NameText);

	InfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InfoText"));
	FSlateFontInfo InfoFont = InfoText->GetFont();
	InfoFont.Size = 11;
	InfoText->SetFont(InfoFont);
	InfoText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	Texts->AddChildToVerticalBox(InfoText);

	JoinButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("JoinButton"));
	UTextBlock* JoinText = WidgetTree->ConstructWidget<UTextBlock>();
	JoinText->SetText(LOCTEXT("Join", "Join"));
	JoinText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
	JoinButton->SetContent(JoinText);
	UHorizontalBoxSlot* JoinSlot = Box->AddChildToHorizontalBox(JoinButton);
	JoinSlot->SetVerticalAlignment(VAlign_Center);
}

void UTHSessionEntryWidget::Setup(const FTHSessionResult& InResult)
{
	SessionResult = InResult;
	if (NameText)
	{
		NameText->SetText(SessionResult.HostName.IsEmpty() ? LOCTEXT("UnnamedHost", "Game") : FText::FromString(SessionResult.HostName));
	}
	if (InfoText)
	{
		InfoText->SetText(FText::Format(LOCTEXT("Info", "{0}/{1} players  ·  {2} ms"),
			FText::AsNumber(SessionResult.CurrentPlayers), FText::AsNumber(SessionResult.MaxPlayers), FText::AsNumber(SessionResult.PingMs)));
	}
	ReceiveSetup(SessionResult);
}

void UTHSessionEntryWidget::HandleJoinClicked()
{
	OnJoin.ExecuteIfBound(SessionResult.SearchIndex);
}

#undef LOCTEXT_NAMESPACE

#include "UI/THMainMenuWidget.h"
#include "UI/THAudioSettingsWidget.h"
#include "UI/THSessionEntryWidget.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "TacticalHorror.h"

#define LOCTEXT_NAMESPACE "THMainMenu"

static TAutoConsoleVariable<int32> CVarAutoMenu(
	TEXT("th.AutoMenu"), 0,
	TEXT("Off-screen test helper on the main menu: 1 = host a game, 2 = find games and join the first one (keeps searching). 0 = off."));

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FText& Text, int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		return Block;
	}
}

// Layout

void UTHMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	// No Widget Blueprint layout: build the plain one (it fills the bound widgets)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UTHMainMenuWidget::HandleHostClicked);
	}
	if (MapButton)
	{
		MapButton->OnClicked.AddDynamic(this, &UTHMainMenuWidget::HandleMapClicked);
		MapButton->SetVisibility(HostMaps.Num() > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	UpdateMapText();
	if (FindButton)
	{
		FindButton->OnClicked.AddDynamic(this, &UTHMainMenuWidget::HandleFindClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UTHMainMenuWidget::HandleSettingsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UTHMainMenuWidget::HandleQuitClicked);
	}
}

void UTHMainMenuWidget::BuildLayout()
{
	// Dark full-screen backdrop; menu column on the left, game list on the right
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.012f, 1.0f));
	Backdrop->SetPadding(FMargin(80.0f, 70.0f));
	WidgetTree->RootWidget = Backdrop;

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Backdrop->SetContent(Columns);

	// Left: title and buttons
	USizeBox* MenuSize = WidgetTree->ConstructWidget<USizeBox>();
	MenuSize->SetWidthOverride(320.0f);
	Columns->AddChildToHorizontalBox(MenuSize);

	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>();
	MenuSize->SetContent(Menu);

	UVerticalBoxSlot* TitleSlot = Menu->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("TitleText"), LOCTEXT("Title", "Let's Survive Together"), 28));
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 40.0f));

	HostButton = AddMenuButton(Menu, TEXT("HostButton"), LOCTEXT("Host", "Host Game"));
	MapButton = AddMenuButton(Menu, TEXT("MapButton"), FText::GetEmpty());
	MapText = Cast<UTextBlock>(MapButton->GetContent());
	FindButton = AddMenuButton(Menu, TEXT("FindButton"), LOCTEXT("Find", "Find Games"));
	SettingsButton = AddMenuButton(Menu, TEXT("SettingsButton"), LOCTEXT("Settings", "Settings"));
	QuitButton = AddMenuButton(Menu, TEXT("QuitButton"), LOCTEXT("Quit", "Quit"));

	StatusText = MakeText(WidgetTree, TEXT("StatusText"), FText::GetEmpty(), 13);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)));
	StatusText->SetAutoWrapText(true);
	UVerticalBoxSlot* StatusSlot = Menu->AddChildToVerticalBox(StatusText);
	StatusSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));

	// Right: found games
	UVerticalBox* GamesColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* GamesSlot = Columns->AddChildToHorizontalBox(GamesColumn);
	GamesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GamesSlot->SetPadding(FMargin(60.0f, 70.0f, 0.0f, 0.0f));

	UVerticalBoxSlot* GamesTitleSlot = GamesColumn->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("GamesTitle"), LOCTEXT("GamesTitle", "Games on your network"), 16));
	GamesTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	UScrollBox* List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SessionList"));
	UVerticalBoxSlot* ListSlot = GamesColumn->AddChildToVerticalBox(List);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SessionList = List;
}

UButton* UTHMainMenuWidget::AddMenuButton(UVerticalBox* Box, FName Name, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* Text = MakeText(WidgetTree, NAME_None, Label, 16);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
	Button->SetContent(Text);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.0f, 6.0f));
	return Button;
}

// Lifetime

void UTHMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UTHSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostComplete.AddUniqueDynamic(this, &UTHMainMenuWidget::HandleHostComplete);
		Sessions->OnFindComplete.AddUniqueDynamic(this, &UTHMainMenuWidget::HandleFindComplete);
		Sessions->OnJoinComplete.AddUniqueDynamic(this, &UTHMainMenuWidget::HandleJoinComplete);
	}

	// Test helper: startup -ExecCmds apply a moment after the menu opens, so look at th.AutoMenu a little later
	GetWorld()->GetTimerManager().SetTimer(AutoMenuTimer, this, &UTHMainMenuWidget::RunAutoMenu, 3.0f, false);
}

void UTHMainMenuWidget::NativeDestruct()
{
	if (UTHSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostComplete.RemoveDynamic(this, &UTHMainMenuWidget::HandleHostComplete);
		Sessions->OnFindComplete.RemoveDynamic(this, &UTHMainMenuWidget::HandleFindComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &UTHMainMenuWidget::HandleJoinComplete);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoMenuTimer);
	}
	Super::NativeDestruct();
}

UTHSessionSubsystem* UTHMainMenuWidget::GetSessions() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTHSessionSubsystem>() : nullptr;
}

void UTHMainMenuWidget::SetStatus(const FText& Status)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
	ReceiveStatusChanged(Status);
}

void UTHMainMenuWidget::SetBusy(bool bBusy)
{
	for (UButton* Button : { HostButton.Get(), FindButton.Get() })
	{
		if (Button)
		{
			Button->SetIsEnabled(!bBusy);
		}
	}
}

// Buttons

void UTHMainMenuWidget::HandleHostClicked()
{
	UTHSessionSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		return;
	}
	SetBusy(true);
	SetStatus(LOCTEXT("Hosting", "Starting a game…"));
	const FString Map = GetHostMap();
	UE_LOG(LogTacticalHorror, Log, TEXT("Main menu: hosting %s for %d players"), *Map, MaxPlayers);
	Sessions->HostSession(Map, MaxPlayers, true);
}

FString UTHMainMenuWidget::GetHostMap() const
{
	return HostMaps.IsValidIndex(MapIndex) ? HostMaps[MapIndex] : HostMapPath;
}

void UTHMainMenuWidget::HandleMapClicked()
{
	if (!HostMaps.IsEmpty())
	{
		MapIndex = (MapIndex + 1) % HostMaps.Num();
		UpdateMapText();
	}
}

void UTHMainMenuWidget::UpdateMapText()
{
	if (MapText)
	{
		// The asset name without its L_ prefix: /Game/Maps/L_FiringRange -> FiringRange
		FString Name = FPackageName::GetShortName(GetHostMap());
		Name.RemoveFromStart(TEXT("L_"));
		MapText->SetText(FText::Format(LOCTEXT("MapLabel", "Map: {0}"), FText::FromString(Name)));
	}
}

void UTHMainMenuWidget::HandleFindClicked()
{
	UTHSessionSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		return;
	}
	if (SessionList)
	{
		SessionList->ClearChildren();
	}
	SetBusy(true);
	SetStatus(LOCTEXT("Searching", "Searching for games…"));
	Sessions->FindSessions(true);
}

void UTHMainMenuWidget::HandleSettingsClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UTHAudioSettingsWidget>(PC, UTHAudioSettingsWidget::StaticClass());
		SettingsWidget->OnCloseRequested.BindUObject(this, &UTHMainMenuWidget::CloseSettings);
	}
	SettingsWidget->AddToViewport(10);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
	PC->SetInputMode(Mode);
}

void UTHMainMenuWidget::CloseSettings()
{
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
	}
	if (UTHAudioSettingsSubsystem* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTHAudioSettingsSubsystem>() : nullptr)
	{
		Settings->SaveSettings();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(Mode);
	}
}

void UTHMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

// Session results

void UTHMainMenuWidget::HandleHostComplete(bool bWasSuccessful)
{
	UE_LOG(LogTacticalHorror, Log, TEXT("Main menu: host %s"), bWasSuccessful ? TEXT("succeeded, travelling") : TEXT("failed"));
	if (!bWasSuccessful)
	{
		SetBusy(false);
		SetStatus(LOCTEXT("HostFailed", "Couldn't start the game."));
	}
}

void UTHMainMenuWidget::HandleFindComplete(const TArray<FTHSessionResult>& Results, bool bWasSuccessful)
{
	SetBusy(false);
	UE_LOG(LogTacticalHorror, Log, TEXT("Main menu: search %s, %d game(s)"), bWasSuccessful ? TEXT("done") : TEXT("failed"), Results.Num());

	if (SessionList)
	{
		SessionList->ClearChildren();
		const TSubclassOf<UTHSessionEntryWidget> EntryClass = SessionEntryClass ? SessionEntryClass : TSubclassOf<UTHSessionEntryWidget>(UTHSessionEntryWidget::StaticClass());
		for (const FTHSessionResult& Result : Results)
		{
			if (UTHSessionEntryWidget* Entry = CreateWidget<UTHSessionEntryWidget>(this, EntryClass))
			{
				Entry->Setup(Result);
				Entry->OnJoin.BindUObject(this, &UTHMainMenuWidget::JoinGame);
				SessionList->AddChild(Entry);
			}
		}
	}

	if (!bWasSuccessful)
	{
		SetStatus(LOCTEXT("SearchFailed", "Couldn't search for games."));
	}
	else if (Results.IsEmpty())
	{
		SetStatus(LOCTEXT("NoGames", "No games found."));
	}
	else
	{
		SetStatus(FText::Format(LOCTEXT("FoundGames", "{0} {0}|plural(one=game,other=games) found."), FText::AsNumber(Results.Num())));
	}

	// Off-screen test: join the first game, or search again
	if (CVarAutoMenu.GetValueOnGameThread() == 2)
	{
		if (!Results.IsEmpty())
		{
			JoinGame(Results[0].SearchIndex);
		}
		else if (++AutoFindAttempts < 15)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoMenuTimer, this, &UTHMainMenuWidget::HandleFindClicked, 2.0f, false);
		}
	}
}

void UTHMainMenuWidget::JoinGame(int32 SearchIndex)
{
	if (UTHSessionSubsystem* Sessions = GetSessions())
	{
		SetBusy(true);
		SetStatus(LOCTEXT("Joining", "Joining…"));
		UE_LOG(LogTacticalHorror, Log, TEXT("Main menu: joining search result %d"), SearchIndex);
		Sessions->JoinSession(SearchIndex);
	}
}

void UTHMainMenuWidget::HandleJoinComplete(bool bWasSuccessful)
{
	UE_LOG(LogTacticalHorror, Log, TEXT("Main menu: join %s"), bWasSuccessful ? TEXT("succeeded, travelling") : TEXT("failed"));
	if (!bWasSuccessful)
	{
		SetBusy(false);
		SetStatus(LOCTEXT("JoinFailed", "Couldn't join that game."));
	}
}

void UTHMainMenuWidget::RunAutoMenu()
{
	switch (CVarAutoMenu.GetValueOnGameThread())
	{
	case 1:
		HandleHostClicked();
		break;
	case 2:
		HandleFindClicked();
		break;
	default:
		break;
	}
}

#undef LOCTEXT_NAMESPACE

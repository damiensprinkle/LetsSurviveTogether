#include "UI/THStatusWidget.h"
#include "Core/THGameState.h"
#include "Player/Components/THHealthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "THStatus"

namespace
{
	/** Splatters that can show at once (older ones are reused). */
	constexpr int32 SplatterCount = 6;

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetJustification(ETextJustify::Center);
		Block->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		return Block;
	}
}

UTHStatusWidget::UTHStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Made by Scripts/Tools/setup_hit_feedback.py; masks from Yarrawah Interactive's blood decals
	BloodMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UI/M_UI_ScreenBlood.M_UI_ScreenBlood")));
	for (const TCHAR* Name : { TEXT("01"), TEXT("02"), TEXT("03"), TEXT("04"), TEXT("05"), TEXT("06") })
	{
		const FString Path = FString::Printf(TEXT("/Game/ThirdParty/YI_BloodDecals/Textures/Singles/T_Blood_Splatter_%s_Single_Mask.T_Blood_Splatter_%s_Single_Mask"), Name, Name);
		BloodMasks.Add(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)));
	}
}

ETHPlayerStatus UTHStatusWidget::GetStatusFor(const APlayerController* PC)
{
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ATHGameState* GameState = World ? World->GetGameState<ATHGameState>() : nullptr;
	if (GameState && GameState->GetMissionPhase() == ETHMissionPhase::Failed)
	{
		return ETHPlayerStatus::MissionFailed;
	}
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UTHHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
	if (!Health)
	{
		return ETHPlayerStatus::None;
	}
	switch (Health->GetHealthState())
	{
	case ETHHealthState::Downed: return ETHPlayerStatus::Downed;
	case ETHHealthState::Dead: return ETHPlayerStatus::Dead;
	default: return ETHPlayerStatus::None;
	}
}

bool UTHStatusWidget::IsOwnPlayerDead() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UTHHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
	return Health && Health->GetHealthState() == ETHHealthState::Dead;
}

void UTHStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (StatusLayer)
	{
		StatusLayer->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UTHStatusWidget::BuildLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Hit: blood splatters (placed when shown) and a red flash over the whole screen
	BloodCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BloodCanvas"));
	UOverlaySlot* CanvasSlot = Root->AddChildToOverlay(BloodCanvas);
	CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
	CanvasSlot->SetVerticalAlignment(VAlign_Fill);

	UMaterialInterface* Material = BloodMaterial.LoadSynchronous();
	for (int32 Index = 0; Index < SplatterCount; ++Index)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		UMaterialInstanceDynamic* Instance = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
		if (Instance)
		{
			Image->SetBrushFromMaterial(Instance);
		}
		Image->SetVisibility(ESlateVisibility::Hidden);
		UCanvasPanelSlot* ImageSlot = BloodCanvas->AddChildToCanvas(Image);
		ImageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BloodImages.Add(Image);
		BloodMaterials.Add(Instance);
		BloodAges.Add(1.0e6f);
		BloodStrengths.Add(0.0f);
	}

	FlashBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FlashBorder"));
	FlashBorder->SetBrushColor(FLinearColor(0.5f, 0.0f, 0.0f, 0.0f));
	UOverlaySlot* FlashSlot = Root->AddChildToOverlay(FlashBorder);
	FlashSlot->SetHorizontalAlignment(HAlign_Fill);
	FlashSlot->SetVerticalAlignment(VAlign_Fill);

	// Status: the whole screen tinted, and the message a little above the middle, clear of the crosshair spot
	UOverlay* Status = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("StatusLayer"));
	UOverlaySlot* StatusSlot = Root->AddChildToOverlay(Status);
	StatusSlot->SetHorizontalAlignment(HAlign_Fill);
	StatusSlot->SetVerticalAlignment(VAlign_Fill);
	StatusLayer = Status;

	TintBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TintBorder"));
	TintBorder->SetBrushColor(DownedTint);
	UOverlaySlot* TintSlot = Status->AddChildToOverlay(TintBorder);
	TintSlot->SetHorizontalAlignment(HAlign_Fill);
	TintSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* BoxSlot = Status->AddChildToOverlay(Box);
	BoxSlot->SetHorizontalAlignment(HAlign_Center);
	BoxSlot->SetVerticalAlignment(VAlign_Center);
	BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 260.0f));

	TitleText = MakeText(WidgetTree, TEXT("TitleText"), 44);
	Box->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);

	DetailText = MakeText(WidgetTree, TEXT("DetailText"), 18);
	DetailText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
	UVerticalBoxSlot* DetailSlot = Box->AddChildToVerticalBox(DetailText);
	DetailSlot->SetHorizontalAlignment(HAlign_Center);
	DetailSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
}

void UTHStatusWidget::ShowHit(float Strength)
{
	Strength = FMath::Clamp(Strength, 0.1f, 1.5f);
	FlashAlpha = FMath::Max(FlashAlpha, 0.18f + 0.12f * Strength);

	// One splatter, two for a heavy blow; each near an edge or corner, at a random size and turn
	const int32 Count = Strength >= 0.75f ? 2 : 1;
	// The blood already there stays too: it all clears together once they've gone a while without a hit
	for (int32 Index = 0; Index < BloodImages.Num(); ++Index)
	{
		if (BloodImages[Index] && BloodImages[Index]->GetVisibility() != ESlateVisibility::Hidden)
		{
			BloodAges[Index] = 0.0f;
		}
	}
	const FVector2D Size = BloodCanvas ? BloodCanvas->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	for (int32 Made = 0; Made < Count && BloodImages.Num() > 0; ++Made)
	{
		const int32 Index = NextSplatter;
		NextSplatter = (NextSplatter + 1) % BloodImages.Num();
		UImage* Image = BloodImages[Index];
		UMaterialInstanceDynamic* Material = BloodMaterials[Index];
		if (Material && BloodMasks.Num() > 0)
		{
			if (UTexture2D* Mask = BloodMasks[FMath::RandHelper(BloodMasks.Num())].LoadSynchronous())
			{
				Material->SetTextureParameterValue(TEXT("Mask"), Mask);
			}
			Material->SetVectorParameterValue(TEXT("Tint"), BloodColor);
		}
		const FVector2D Screen = Size.X > 1.0f ? Size : FVector2D(1920.0f, 1080.0f);
		const float Extent = Screen.Y * FMath::FRandRange(0.45f, 0.7f) * FMath::Lerp(0.8f, 1.15f, FMath::Min(Strength, 1.0f));
		// Around the edge: a point on the screen's border, pulled a little inwards
		const float Along = FMath::FRand();
		FVector2D Where;
		switch (FMath::RandHelper(4))
		{
		case 0: Where = FVector2D(Along * Screen.X, 0.0f); break;
		case 1: Where = FVector2D(Along * Screen.X, Screen.Y); break;
		case 2: Where = FVector2D(0.0f, Along * Screen.Y); break;
		default: Where = FVector2D(Screen.X, Along * Screen.Y); break;
		}
		Where = FMath::Lerp(Where, Screen * 0.5f, FMath::FRandRange(0.05f, 0.15f));
		if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			ImageSlot->SetPosition(Where);
			ImageSlot->SetSize(FVector2D(Extent, Extent));
		}
		Image->SetRenderTransformAngle(FMath::FRandRange(0.0f, 360.0f));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		BloodAges[Index] = 0.0f;
		BloodStrengths[Index] = FMath::Clamp(0.55f + 0.3f * Strength, 0.0f, 0.9f);
	}
	ReceiveHit(Strength);
}

float UTHStatusWidget::GetBloodHold() const
{
	// Held until their health starts coming back (its regeneration delay), else BloodHoldTime
	const APawn* Pawn = GetOwningPlayerPawn();
	const UTHHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
	return Health && Health->Regenerates() ? Health->GetRegenDelay() : BloodHoldTime;
}

int32 UTHStatusWidget::GetVisibleBloodCount() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < BloodImages.Num(); ++Index)
	{
		Count += BloodImages[Index] && BloodImages[Index]->GetVisibility() != ESlateVisibility::Hidden ? 1 : 0;
	}
	return Count;
}

void UTHStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateHit(MyGeometry, InDeltaTime);
	UpdateStatus(InDeltaTime);
}

void UTHStatusWidget::UpdateHit(const FGeometry& MyGeometry, float DeltaTime)
{
	if (FlashBorder)
	{
		FlashAlpha = FMath::FInterpConstantTo(FlashAlpha, 0.0f, DeltaTime, 0.8f);
		FlashBorder->SetBrushColor(FLinearColor(0.5f, 0.0f, 0.0f, FlashAlpha));
	}
	const float Hold = GetBloodHold();
	for (int32 Index = 0; Index < BloodImages.Num(); ++Index)
	{
		UImage* Image = BloodImages[Index];
		if (!Image || Image->GetVisibility() == ESlateVisibility::Hidden)
		{
			continue;
		}
		BloodAges[Index] += DeltaTime;
		const float Fade = 1.0f - FMath::Clamp((BloodAges[Index] - Hold) / FMath::Max(BloodFadeTime, 0.05f), 0.0f, 1.0f);
		if (Fade <= 0.0f)
		{
			Image->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}
		if (UMaterialInstanceDynamic* Material = BloodMaterials[Index])
		{
			Material->SetScalarParameterValue(TEXT("Opacity"), BloodStrengths[Index] * Fade);
		}
		else
		{
			Image->SetOpacity(BloodStrengths[Index] * Fade);
		}
	}
}

void UTHStatusWidget::UpdateStatus(float DeltaTime)
{
	const APlayerController* PC = GetOwningPlayer();
	const ETHPlayerStatus Status = GetStatusFor(PC);

	// Downed: the countdown, and the tint breathing slowly
	int32 Seconds = -1;
	if (Status == ETHPlayerStatus::Downed)
	{
		const APawn* Pawn = PC->GetPawn();
		const UTHHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
		Seconds = Health ? FMath::CeilToInt(Health->GetBleedOutTimeRemaining()) : -1;
		PulseTime += DeltaTime;
		if (TintBorder)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(PulseTime * 2.0f);
			FLinearColor Tint = DownedTint;
			Tint.A = DownedTint.A * (0.8f + 0.2f * Pulse);
			TintBorder->SetBrushColor(Tint);
		}
	}

	if (Status == ShownStatus && Seconds == ShownSeconds)
	{
		return;
	}
	const bool bChanged = Status != ShownStatus;
	ShownStatus = Status;
	ShownSeconds = Seconds;

	if (StatusLayer)
	{
		StatusLayer->SetVisibility(Status == ETHPlayerStatus::None ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}

	FText Title;
	FText Detail;
	switch (Status)
	{
	case ETHPlayerStatus::Downed:
		Title = LOCTEXT("Downed", "YOU'RE DOWN");
		Detail = Seconds >= 0
			? FText::Format(LOCTEXT("DownedDetail", "Bleeding out in {0}s. A teammate can revive you."), FText::AsNumber(Seconds))
			: LOCTEXT("DownedDetailNoTime", "A teammate can revive you.");
		break;
	case ETHPlayerStatus::Dead:
		Title = LOCTEXT("Dead", "YOU DIED");
		break;
	case ETHPlayerStatus::MissionFailed:
		Title = IsOwnPlayerDead() ? LOCTEXT("FailedDead", "YOU DIED") : LOCTEXT("Failed", "EVERYONE IS DOWN");
		Detail = LOCTEXT("FailedDetail", "Mission failed. Restarting...");
		break;
	default:
		break;
	}
	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetColorAndOpacity(FSlateColor(Status == ETHPlayerStatus::Downed ? FLinearColor(0.95f, 0.25f, 0.2f, 1.0f) : FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
	}
	if (DetailText)
	{
		DetailText->SetText(Detail);
		DetailText->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TintBorder && Status != ETHPlayerStatus::Downed)
	{
		TintBorder->SetBrushColor(DeadTint);
	}
	if (bChanged)
	{
		PulseTime = 0.0f;
		ReceiveStatusChanged(Status);
	}
}

#undef LOCTEXT_NAMESPACE

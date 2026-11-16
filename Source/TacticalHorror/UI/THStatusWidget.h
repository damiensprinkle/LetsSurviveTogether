#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "THStatusWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UOverlay;
class UTextBlock;
class UTexture2D;

/** What the status screen is showing. */
UENUM(BlueprintType)
enum class ETHPlayerStatus : uint8
{
	None,
	Downed,
	Dead,
	MissionFailed
};

/**
 * Full screen, for the local player:
 * - Hit: a quick red flash and blood splattered near the edges of the screen, staying until health starts coming back, then fading (ShowHit, from
 *   ATHPlayerController::NotifyLocalHit). The view jolt is UTHCameraModifier's.
 * - Status: downed (red tint, "You're down", bleed-out countdown), dead, and the mission failed when everyone is down
 *   (the mission restarts a few seconds later). Hidden while alive.
 *
 * Works on its own with a plain layout built in code (placeholder look, see AI_CONTENT.md). A Widget Blueprint based
 * on this class can use its own widgets named TintBorder, TitleText and DetailText (all optional); set it as the player
 * controller's Status Widget Class. ReceiveStatusChanged / ReceiveHit let a Blueprint add anything else.
 */
UCLASS(Config = Game)
class TACTICALHORROR_API UTHStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTHStatusWidget(const FObjectInitializer& ObjectInitializer);

	ETHPlayerStatus GetShownStatus() const { return ShownStatus; }

	/** The status for the player of this controller right now. */
	static ETHPlayerStatus GetStatusFor(const APlayerController* PC);

	/** The local player was hit; Strength 1 is a heavy blow (about 40 damage). */
	void ShowHit(float Strength);

	/** Splatters showing now (for tests). */
	int32 GetVisibleBloodCount() const;

	/** Seconds the blood stays after the last hit before fading: the player's regeneration delay, else BloodHoldTime. */
	float GetBloodHold() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FLinearColor DownedTint = FLinearColor(0.35f, 0.0f, 0.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FLinearColor DeadTint = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	/** UI material drawing one splatter: texture parameter "Mask" (its red is the coverage), vector "Tint", scalar "Opacity". */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TSoftObjectPtr<UMaterialInterface> BloodMaterial;

	/** Splatter masks, one picked at random per splatter. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TArray<TSoftObjectPtr<UTexture2D>> BloodMasks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	FLinearColor BloodColor = FLinearColor(0.32f, 0.0f, 0.0f, 1.0f);

	/**
	 * Seconds the blood stays fully visible after the last hit, then fades over BloodFadeTime. A player who heals by
	 * themselves uses their health's regeneration delay instead, so the blood clears as healing starts (2026-11-12).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Units = "s"))
	float BloodHoldTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Units = "s"))
	float BloodFadeTime = 2.2f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Status", meta = (DisplayName = "On Status Changed"))
	void ReceiveStatusChanged(ETHPlayerStatus NewStatus);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hit", meta = (DisplayName = "On Hit"))
	void ReceiveHit(float Strength);

	UPROPERTY(BlueprintReadOnly, Category = "Status", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TintBorder;

	UPROPERTY(BlueprintReadOnly, Category = "Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

private:
	void BuildLayout();
	void UpdateHit(const FGeometry& MyGeometry, float DeltaTime);
	void UpdateStatus(float DeltaTime);
	bool IsOwnPlayerDead() const;

	/** The status part (tint and text), shown only when there is a status; the hit layer is always there. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StatusLayer;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FlashBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BloodCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BloodImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BloodMaterials;

	TArray<float> BloodAges;
	TArray<float> BloodStrengths;
	int32 NextSplatter = 0;
	float FlashAlpha = 0.0f;

	ETHPlayerStatus ShownStatus = ETHPlayerStatus::None;
	int32 ShownSeconds = -1;
	float PulseTime = 0.0f;
};

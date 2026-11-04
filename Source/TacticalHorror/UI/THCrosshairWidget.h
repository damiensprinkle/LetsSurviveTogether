#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "THCrosshairWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * Crosshair in the middle of the screen: the player's chosen style, size and colour (UTHCrosshairSettingsSubsystem).
 * Shown while holding a weapon and looking through your own eyes; hidden while aiming down sights (the sights take
 * over), without a weapon, and in the outside debug view.
 *
 * The styles players can pick from are listed in DefaultGame.ini:
 *   [/Script/TacticalHorror.THCrosshairWidget]
 *   +Styles=/Game/UI/Crosshairs/T_CH001.T_CH001
 * White images with a dark outline work best: the colour tints the white and the outline keeps it readable.
 * A Widget Blueprint based on this class can supply its own image named CrosshairImage.
 */
UCLASS(Config = Game)
class TACTICALHORROR_API UTHCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	TArray<TSoftObjectPtr<UTexture2D>> Styles;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Crosshair", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CrosshairImage;

private:
	void ApplySettings();
	bool ShouldShow() const;

	/**
	 * The template's HUD (WB_HUD → WB_Crosshair) draws its own dot, and a hand when looking at something usable.
	 * This crosshair replaces the dot (kept hidden) and steps aside while the hand shows.
	 */
	void FindTemplateCrosshair();
	bool IsTemplateHandShowing() const;

	FDelegateHandle ChangedHandle;
	TWeakObjectPtr<UImage> TemplateDot;
	TWeakObjectPtr<UImage> TemplateHand;
	float TemplateSearchCooldown = 0.0f;
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "THAudioSettingsWidget.generated.h"

class UVerticalBox;
class USlider;
class UTextBlock;

DECLARE_DELEGATE(FTHOnMenuCloseRequested);

/**
 * Simple audio settings panel: one slider per volume in UTHAudioSettingsSubsystem. The layout is built in code so it works
 * without a Widget Blueprint; a Blueprint subclass can replace it by giving the panel its own widget tree.
 * The labels are AI-written placeholders (see AI_CONTENT.md).
 */
UCLASS()
class TACTICALHORROR_API UTHAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Close button, Escape or P. The player controller owns opening and closing. */
	FTHOnMenuCloseRequested OnCloseRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildLayout();
	USlider* AddVolumeRow(UVerticalBox* Box, const FText& Label, TObjectPtr<UTextBlock>& OutValueText);
	class UTHAudioSettingsSubsystem* GetSettings() const;
	static FText FormatPercent(float Value);

	UFUNCTION()
	void HandleMasterChanged(float Value);

	UFUNCTION()
	void HandleGunshotChanged(float Value);

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> GunshotSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MasterValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GunshotValueText;
};

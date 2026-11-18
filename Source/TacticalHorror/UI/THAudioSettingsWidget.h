#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "THAudioSettingsWidget.generated.h"

class UVerticalBox;
class USlider;
class UCheckBox;
class UTextBlock;

DECLARE_DELEGATE(FTHOnMenuCloseRequested);

/**
 * Settings panel (P / Escape in game, Settings on the main menu). Controls: look and aim-down-sights sensitivity,
 * from UTHControlsSettingsSubsystem, and a converter from other games' sensitivity. Audio: one slider per volume in
 * UTHAudioSettingsSubsystem, and the 3D audio (binaural) toggle. Crosshair: style (previous / next with a live preview), size and R / G / B colour, from
 * UTHCrosshairSettingsSubsystem. (The class keeps its original "audio" name; it is the general settings panel.)
 * The layout is built in code so it works without a Widget Blueprint; a Blueprint subclass can replace it by giving
 * the panel its own widget tree. The labels are AI-written placeholders (see AI_CONTENT.md).
 */
UCLASS()
class TACTICALHORROR_API UTHAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Close button, Escape or P. The player controller owns opening and closing. */
	FTHOnMenuCloseRequested OnCloseRequested;

	/** Leave Game button (in game only; set bShowLeaveGame before adding the panel to the screen). */
	FTHOnMenuCloseRequested OnLeaveGameRequested;
	bool bShowLeaveGame = false;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	/** Saves the controls and crosshair choices when the panel closes (audio is saved by whoever closes it). */
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildLayout();
	void AddSectionTitle(UVerticalBox* Box, const FText& Title);
	USlider* AddSliderRow(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, TObjectPtr<UTextBlock>& OutValueText);
	UCheckBox* AddToggleRow(UVerticalBox* Box, const FText& Label);
	USlider* AddVolumeRow(UVerticalBox* Box, const FText& Label, TObjectPtr<UTextBlock>& OutValueText);
	void AddCrosshairStyleRow(UVerticalBox* Box);
	void AddSensitivityConverterRows(UVerticalBox* Box);
	void RefreshConverterGame();
	void RefreshSensitivityControls();
	class UTHAudioSettingsSubsystem* GetSettings() const;
	class UTHCrosshairSettingsSubsystem* GetCrosshairSettings() const;
	class UTHControlsSettingsSubsystem* GetControlsSettings() const;
	class UTHDisplaySettingsSubsystem* GetDisplaySettings() const;
	static FText FormatSensitivity(float Value);
	static FText FormatPercent(float Value);
	void RefreshCrosshairControls();
	void SetColorChannel(int32 Channel, float Value);

	UFUNCTION()
	void HandleMasterChanged(float Value);

	UFUNCTION()
	void HandleGunplayChanged(float Value);

	UFUNCTION()
	void HandleMusicChanged(float Value);

	UFUNCTION()
	void HandleSpatialAudioChanged(bool bIsChecked);

	UFUNCTION()
	void HandleBloodChanged(bool bIsChecked);

	UFUNCTION()
	void HandleLookSensitivityChanged(float Value);

	UFUNCTION()
	void HandleAimSensitivityChanged(float Value);

	UFUNCTION()
	void HandlePreviousConverterGame();

	UFUNCTION()
	void HandleNextConverterGame();

	UFUNCTION()
	void HandleConvertClicked();

	UFUNCTION()
	void HandleTheirSensitivityCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandlePreviousStyle();

	UFUNCTION()
	void HandleNextStyle();

	UFUNCTION()
	void HandleSizeChanged(float Value);

	UFUNCTION()
	void HandleRedChanged(float Value);

	UFUNCTION()
	void HandleGreenChanged(float Value);

	UFUNCTION()
	void HandleBlueChanged(float Value);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleLeaveGameClicked();

	UPROPERTY(Transient)
	TObjectPtr<class UImage> CrosshairPreview;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StyleText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SizeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SizeValueText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> ColorSliders[3];

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ColorValueTexts[3];

	UPROPERTY(Transient)
	TObjectPtr<USlider> LookSensitivitySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LookSensitivityValueText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> AimSensitivitySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AimSensitivityValueText;

	/** Sensitivity converter: which game (index into UTHControlsSettingsSubsystem::GetOtherGames), their value, the outcome. */
	int32 ConverterGame = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConverterGameText;

	UPROPERTY(Transient)
	TObjectPtr<class UEditableTextBox> TheirSensitivityBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConverterResultText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> GunplaySlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> SpatialAudioCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> BloodCheck;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MasterValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GunplayValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MusicValueText;
};

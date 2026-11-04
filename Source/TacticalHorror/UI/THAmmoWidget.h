#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "THAmmoWidget.generated.h"

class UTextBlock;
class ATHWeaponBase;

/**
 * Ammo readout, bottom right: rounds in the magazine, reserve, and fire mode (AUTO / SEMI, only for weapons that can
 * switch). Shown while holding a weapon and looking through your own eyes. The magazine count turns red when low.
 *
 * Works on its own with a plain layout built in code (placeholder text, see AI_CONTENT.md). A Widget Blueprint based
 * on this class can use its own widgets named MagazineText, ReserveText and FireModeText (all optional); set it as
 * the player controller's Ammo Widget Class. ReceiveAmmoChanged lets a Blueprint show it any other way.
 */
UCLASS()
class TACTICALHORROR_API UTHAmmoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** At or below this share of a full magazine, the magazine count turns LowAmmoColor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 0, ClampMax = 1))
	float LowAmmoFraction = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FLinearColor NormalColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FLinearColor LowAmmoColor = FLinearColor(0.9f, 0.2f, 0.15f, 1.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ammo", meta = (DisplayName = "On Ammo Changed"))
	void ReceiveAmmoChanged(int32 Magazine, int32 Reserve, int32 MagazineSize, bool bReloading);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MagazineText;

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReserveText;

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FireModeText;

private:
	void BuildLayout();
	ATHWeaponBase* GetShownWeapon() const;

	/** Last shown values, so the texts only change when the ammo does. */
	TWeakObjectPtr<ATHWeaponBase> ShownWeapon;
	int32 ShownMagazine = -1;
	int32 ShownReserve = -1;
	int32 ShownFireMode = -1;
	bool bShownReloading = false;
};

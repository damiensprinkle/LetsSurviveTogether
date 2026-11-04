#pragma once

#include "CoreMinimal.h"
#include "Weapons/THWeaponBase.h"
#include "THHandFlashlight.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USpotLightComponent;

/**
 * The hand flashlight: a held item in the weapon slots (key 3 once the player has picked one up). Brighter than the
 * weapon light. Left mouse switches it on and off.
 *
 * First person: the arms of the FP Flashlight Animations pack (idle, draw, inspect) with the flashlight model in the
 * arms' hand_r_Flashlight socket. Everyone else sees the body hold it (DA_Body_Flashlight, Lyra flashlight poses) with
 * a copy of the model in the body's right hand at the same socket offset.
 *
 * Mesh, animations and body set come from the weapon data (DA_HandFlashlight); the model and the beam are set here.
 */
UCLASS(Blueprintable)
class TACTICALHORROR_API ATHHandFlashlight : public ATHWeaponBase
{
	GENERATED_BODY()

public:
	ATHHandFlashlight();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnEquipped() override;
	virtual void OnUnequipped() override;

	/** Switches the light (left mouse). */
	virtual void StartFire() override;
	virtual void StopFire() override {}
	virtual void StartReload() override {}
	virtual bool IsFirearm() const override { return false; }

	/** Plays the inspect animation (first person). */
	void PlayInspect();

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsLightOn() const { return bLightOn; }

protected:
	virtual void BeginPlay() override;
	virtual bool UsesThirdPersonWeaponCopy() const override { return false; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeldFlashlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ThirdPersonFlashlight;

	/** One beam per machine, on the copy this machine sees. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Beam;

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight")
	TObjectPtr<UStaticMesh> FlashlightMesh;

	/** Socket on the first-person arms the model goes in; also its offset from the body's ThirdPersonHandBone. */
	UPROPERTY(EditDefaultsOnly, Category = "Flashlight")
	FName HandSocket = TEXT("hand_r_Flashlight");

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight")
	FName ThirdPersonHandBone = TEXT("hand_r");

	/** Nudges the third-person model from the socket offset, in the hand bone's space. */
	UPROPERTY(EditDefaultsOnly, Category = "Flashlight")
	FTransform ThirdPersonAdjust;

	/** Where the beam leaves the model, in the model's space; X of this transform is the beam direction. */
	UPROPERTY(EditDefaultsOnly, Category = "Flashlight")
	FTransform BeamOffset = FTransform(FRotator(0.0f, -90.0f, 0.0f), FVector(0.0f, -7.6f, -0.5f));

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight")
	TObjectPtr<UAnimSequenceBase> InspectAnimation;

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight|Beam", meta = (ClampMin = 0))
	float Intensity = 10000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight|Beam", meta = (ClampMin = 0, Units = "cm"))
	float Range = 3500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight|Beam", meta = (ClampMin = 0, ClampMax = 80, Units = "Degrees"))
	float InnerCone = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight|Beam", meta = (ClampMin = 0, ClampMax = 80, Units = "Degrees"))
	float OuterCone = 38.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flashlight|Beam")
	FLinearColor Color = FLinearColor(1.0f, 0.9f, 0.82f);

private:
	void AttachThirdPersonFlashlight();
	void UpdateBeam();

	UFUNCTION(Server, Reliable)
	void ServerSetLightOn(bool bOn);

	UPROPERTY(Replicated)
	bool bLightOn = true;
};

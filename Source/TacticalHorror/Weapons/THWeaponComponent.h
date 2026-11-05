#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "THWeaponComponent.generated.h"

class ATHWeaponBase;
class UAnimInstance;
class UCameraComponent;
class USkeletalMeshComponent;
class UTHBodyAnimInstance;
class UTHBodyAnimSet;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTHWeaponChanged, ATHWeaponBase*, NewWeapon, ATHWeaponBase*, OldWeapon);

/**
 * Holds the pawn's weapons: spawns the loadout, switches weapons, aims down sights and routes weapon input.
 *
 * Input: assign WeaponMappingContext and the actions to use your own assets. If no mapping context is assigned,
 * a default one is built at runtime (LMB fire, RMB aim, R reload, 1/2 slots, wheel cycle, B fire mode).
 * It is added above the character's own context, so RMB aims instead of the horror template's camera zoom.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTHWeaponComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called by the owning character when its input component is set up. */
	void SetupPlayerInput(UEnhancedInputComponent* InputComponent);

	UFUNCTION(BlueprintPure, Category = "Weapons")
	ATHWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintPure, Category = "Weapons")
	const TArray<ATHWeaponBase*>& GetWeapons() const { return ObjectPtrDecay(Weapons); }

	UFUNCTION(BlueprintPure, Category = "Weapons")
	bool IsAiming() const { return bIsAiming; }

	/** 0 = hip, 1 = fully aimed. Local player only. */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	float GetAimAlpha() const { return AimAlpha; }

	/** Multiplier on max walk speed from the held weapon and aiming. */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	float GetMovementSpeedMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void EquipWeaponAtIndex(int32 Index);

	/** Stops firing and aiming, e.g. while downed. Call on every machine (it is driven by replicated state). */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void SetWeaponsBlocked(bool bBlocked);

	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FTHWeaponChanged OnWeaponChanged;

	/** The full-body mesh other players see: the character's Mesh if it has one, else its first skeletal mesh with an asset. */
	UFUNCTION(BlueprintPure, Category = "Weapons|Body")
	USkeletalMeshComponent* GetBodyMesh() const;

	UTHBodyAnimInstance* GetBodyAnimInstance() const;

	/** The held weapon's body animations, or null when unarmed. */
	UTHBodyAnimSet* GetHeldBodyAnimSet() const;

	UTHBodyAnimSet* GetUnarmedBodyAnimSet() const { return UnarmedBodyAnimSet; }

	// Weapon light (L): an under-barrel light on guns whose data has one. An upgrade: off until unlocked.

	UFUNCTION(BlueprintPure, Category = "Weapons|Light")
	bool IsWeaponLightUnlocked() const { return bWeaponLightUnlocked; }

	UFUNCTION(BlueprintPure, Category = "Weapons|Light")
	bool IsWeaponLightOn() const { return bWeaponLightUnlocked && bWeaponLightOn; }

	/** Server only. E.g. a mission reward. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapons|Light")
	void UnlockWeaponLight();

	UFUNCTION(BlueprintCallable, Category = "Weapons|Light")
	void ToggleWeaponLight();

	// Hand flashlight (3): a held item, given when the player picks up a flashlight

	UFUNCTION(BlueprintPure, Category = "Weapons|Flashlight")
	bool HasHandFlashlight() const;

	/** Gives the hand flashlight (server; from the owning client it asks the server). bInspect: take it out and look it over. */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Flashlight")
	void GiveHandFlashlight(bool bInspect);

protected:
	/** Held item given by GiveHandFlashlight. It goes in the next free slot (the third, key 3, after the two guns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons|Flashlight")
	TSubclassOf<ATHWeaponBase> HandFlashlightClass;

	/**
	 * Bool on the owner (the template character's PickupFlashLight) that turns true when it picks up a flashlight.
	 * Watched on the owning client; when it flips, the hand flashlight is given and inspected. None = not watched.
	 */
	UPROPERTY(EditAnywhere, Category = "Weapons|Flashlight")
	FName FlashlightPickupFlag = TEXT("PickupFlashLight");

	/** Start with the weapon light unlocked. Off once missions can award it. */
	UPROPERTY(EditAnywhere, Category = "Weapons|Light")
	bool bStartWithWeaponLight = true;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Spawned on the server at BeginPlay, in slot order. The first one is equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TArray<TSubclassOf<ATHWeaponBase>> DefaultLoadout;

	/** Put on the body mesh at BeginPlay so other players see weapons held. Empty = keep the body's own Anim Blueprint. */
	UPROPERTY(EditAnywhere, Category = "Body Animation")
	TSubclassOf<UAnimInstance> BodyAnimClass;

	/**
	 * The body others see blocks bullets (BulletTraceChannel only) with its physics asset, so shots hit bones; nothing
	 * else collides with it. Needed because bullets pass through the capsule.
	 */
	UPROPERTY(EditAnywhere, Category = "Body Animation")
	bool bBodyBlocksBullets = true;

	/** The trace channel bullets use: "Weapon" in DefaultEngine.ini (as ATHWeaponBase::TraceChannel). */
	UPROPERTY(EditAnywhere, Category = "Body Animation")
	TEnumAsByte<ECollisionChannel> BulletTraceChannel = ECC_GameTraceChannel1;

	/** Body animations with no weapon held, and the legs for weapon sets that have none. */
	UPROPERTY(EditAnywhere, Category = "Body Animation")
	TObjectPtr<UTHBodyAnimSet> UnarmedBodyAnimSet;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> WeaponMappingContext;

	/** Above the character's own mapping context so weapon keys win. */
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 MappingContextPriority = 1;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> NextWeaponAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PreviousWeaponAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CycleFireModeAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> WeaponLightAction;

	/** Index N equips weapon N. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<TObjectPtr<UInputAction>> EquipSlotActions;

private:
	void BuildDefaultInput();
	void AddMappingContext();
	void RemoveMappingContext();

	void SetCurrentWeapon(ATHWeaponBase* NewWeapon);
	void SetAiming(bool bNewAiming);
	void CycleWeapon(int32 Direction);
	UCameraComponent* FindViewCamera() const;
	void UpdateAimFieldOfView(float DeltaTime);

	void OnFirePressed();
	void OnFireReleased();
	void OnAimPressed();
	void OnAimReleased();
	void OnReloadPressed();
	void OnNextWeapon();
	void OnPreviousWeapon();
	void OnCycleFireMode();
	void OnEquipSlot(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon(ATHWeaponBase* NewWeapon);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetWeaponLightOn(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerGiveHandFlashlight(bool bEquip);

	/** Watches FlashlightPickupFlag on the owning client. */
	void CheckFlashlightPickup();

	UPROPERTY(Replicated)
	bool bWeaponLightUnlocked = false;

	UPROPERTY(Replicated)
	bool bWeaponLightOn = false;

	/** Owning client: inspect the hand flashlight when it next becomes the held item. */
	bool bInspectFlashlightOnEquip = false;
	bool bRequestedHandFlashlight = false;

	UFUNCTION()
	void OnRep_CurrentWeapon(ATHWeaponBase* OldWeapon);

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ATHWeaponBase>> Weapons;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<ATHWeaponBase> CurrentWeapon;

	UPROPERTY(Replicated)
	bool bIsAiming = false;

	bool bWeaponsBlocked = false;

	// Local aim presentation
	float AimAlpha = 0.0f;
	float HipFieldOfView = 90.0f;
	bool bAppliedAimFieldOfView = false;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
};

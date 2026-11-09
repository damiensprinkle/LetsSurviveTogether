#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/THWeaponTypes.h"
#include "THWeaponBase.generated.h"

class APawn;
class UAnimSequenceBase;
class USoundBase;
class USkeletalMeshComponent;
class UTHWeaponData;

/**
 * Hitscan weapon. Behaviour lives here; tuning lives in UTHWeaponData; mesh and animation live in Blueprint subclasses.
 *
 * Networking: the owning client fires immediately (recoil, effects, predicted ammo) and sends the shot to the server.
 * The server validates fire rate, ammo and origin, re-traces with the same deterministic recoil and spread, applies damage,
 * and multicasts cosmetics to everyone else. A listen-server host takes the same path without the RPC hop.
 */
UCLASS(Abstract, Blueprintable)
class TACTICALHORROR_API ATHWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ATHWeaponBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called on every machine by UTHWeaponComponent when this becomes / stops being the held weapon. */
	virtual void OnEquipped();
	virtual void OnUnequipped();

	/** Local input from the owning player. */
	virtual void StartFire();
	virtual void StopFire();
	virtual void StartReload();
	void CycleFireMode();

	/** False for held items that aren't guns (the hand flashlight): no ammo readout, crosshair or aiming down sights. */
	virtual bool IsFirearm() const { return true; }

	/** Whether this weapon has an under-barrel light (its data has a mesh for it). */
	bool HasWeaponLight() const;

	USoundBase* GetLightClickSound() const;

	/** Blocks firing without unequipping, e.g. while downed. */
	void SetFiringBlocked(bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UTHWeaponData* GetWeaponData() const { return WeaponData; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	ETHFireMode GetFireMode() const;

	/** Current spread cone half-angle in degrees. Useful for a dynamic crosshair. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetCurrentInaccuracy() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

protected:
	virtual void BeginPlay() override;

	/** Where the weapon goes when equipped. Default: held in front of the owner's active camera. */
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void AttachToOwner();

	// Blueprint hooks for animation and presentation

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Fired"))
	void ReceiveFired();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Reload Started"))
	void ReceiveReloadStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Reload Finished"))
	void ReceiveReloadFinished();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Equipped"))
	void ReceiveEquipped();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Unequipped"))
	void ReceiveUnequipped();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon", meta = (DisplayName = "On Fire Mode Changed"))
	void ReceiveFireModeChanged(ETHFireMode NewMode);

	/** First-person weapon (with arms), held in front of the owner's camera. Only the owner sees it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/**
	 * What everyone else sees: the same mesh (unless set otherwise) in the body's hand, arms hidden, pointed where the
	 * owner aims. The owner never sees it. Set up from the Third Person section of the weapon data.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UPointLightComponent> MuzzleLight;

	/** One quad facing down the barrel and two crossed along it, so the flash reads from behind and from the side. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<class UStaticMeshComponent>> MuzzleFlashQuads;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UTHWeaponData> WeaponData;

	/** Trace channel for bullets. "Weapon" is defined in DefaultEngine.ini; pawn capsules ignore it so traces reach bones. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;

	/** How far the client's reported shot origin may be from the server's view of the pawn's eyes. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Network", meta = (Units = "cm"))
	float MaxOriginError = 150.0f;

	/** Under-barrel light on the first-person gun (owner only) and on the third-person copy (everyone else). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> WeaponLightMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> ThirdPersonWeaponLightMesh;

	/** One beam per machine, on whichever copy this machine sees. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USpotLightComponent> WeaponLightBeam;

	/** The new magazine in the body's left hand during a third-person reload (everyone but the owner). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> HeldMagazine;

	/** Plays on the first-person mesh, then returns to the idle loop. */
	void PlayWeaponAnimation(UAnimSequenceBase* Animation);

	APawn* GetOwningPawn() const;
	bool IsLocallyControlledOwner() const;

	/** The copy this machine shows: first person for the owner, third person for everyone else. */
	USkeletalMeshComponent* GetVisibleMesh() const;

	/** False: no third-person copy of Mesh in the body's hand (an item that shows something else for others). */
	virtual bool UsesThirdPersonWeaponCopy() const { return true; }

	bool IsEquipped() const { return bIsEquipped; }

	/** True while the body's draw animation hasn't yet brought the hand up (the third-person item stays hidden). */
	bool IsThirdPersonHiddenForDraw() const;

private:
	bool IsAimingDownSights() const;

	void SetupWeaponLight();
	void UpdateWeaponLight();

	/** Third-person reload: magazine out of the gun and dropped, new one in the left hand, then seated. */
	void UpdateThirdPersonMagazine();
	void ResetThirdPersonMagazine();

	double ReloadStartTime = 0.0;
	bool bMagazineDropped = false;
	bool bMagazineBoneHidden = false;
	/** The body's left-hand finger bones, for its palm. */
	TArray<FName> BodyLeftFingers[5];
	TWeakObjectPtr<const USkeletalMesh> BodyLeftFingersMesh;

	void UpdateFirstPersonRendering();
	void HandleFiring();
	bool CanFire() const;
	void FireShot();

	/** Spray index for a shot at Now, given when the previous shot happened. */
	int32 GetSprayIndexForShot(int32 PreviousIndex, float LastShotTime, float Now) const;

	/** Spread cone half-angle in degrees for the given shot of a spray, from the owner's current movement. */
	float GetInaccuracyForSprayIndex(int32 SprayIndex) const;

	/** Deterministic shot direction: same inputs give the same result on client and server. */
	FVector ComputeShotDirection(const FRotator& ViewRotation, int32 SprayIndex, int32 ShotNumber) const;
	bool TraceShot(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const;

	void ApplyViewKick(int32 SprayIndex);
	void UpdateViewRecovery(float DeltaSeconds);

	FTransform GetMuzzleTransform() const;

	void SetupThirdPersonMesh();
	void AttachThirdPersonMesh();
	void UpdateThirdPersonMesh();

	/** Direction cosmetics use (third-person weapon, muzzle flash, casings). */
	static FRotator GetPresentationAim(const APawn* Pawn);

	/** Upper-body animation on the owner's body, from the weapon data's BodyAnimSet. */
	void PlayBodyAction(class UAnimSequence* Animation, float Duration, float BlendIn, float BlendOut);
	void PlayFireEffects();
	void ShowMuzzleFlash(const FTransform& Muzzle);
	void EjectShellCasing(const FTransform& Muzzle);
	/** Surface is an EPhysicalSurface: what the shot hit, which picks the bullet hole. */
	void PlayImpactEffects(const FVector& Location, const FVector& Normal, uint8 Surface);

	void ProcessServerShot(const FVector& ViewLocation, const FRotator& ViewRotation, int32 ClientSprayIndex, int32 ShotNumber);
	void FinishReload();

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize10 ViewLocation, FRotator ViewRotation, int32 ClientSprayIndex, int32 ShotNumber);

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerSetFireModeIndex(uint8 NewIndex);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotEffects(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal, bool bHit, uint8 Surface);

	UFUNCTION()
	void OnRep_IsReloading();

	UFUNCTION()
	void OnRep_FireModeIndex();

	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	UPROPERTY(Replicated)
	int32 ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	bool bIsReloading = false;

	UPROPERTY(ReplicatedUsing = OnRep_FireModeIndex)
	uint8 FireModeIndex = 0;

	/** Shared seed so client and server compute the same spread. */
	UPROPERTY(Replicated)
	int32 SpreadSeed = 0;

	bool bIsEquipped = false;
	bool bFiringBlocked = false;
	bool bWantsToFire = false;
	bool bFiredThisTriggerPull = false;
	float EquipFinishTime = 0.0f;

	// Owning client
	float NextFireTime = 0.0f;
	float LastLocalShotTime = -1000.0f;
	int32 LocalSprayIndex = 0;
	int32 LocalShotNumber = 0;
	FVector2D PendingViewRecovery = FVector2D::ZeroVector;

	// Server
	float LastServerShotTime = -1000.0f;
	int32 ServerSprayIndex = 0;
	int32 LastServerShotNumber = 0;

	FTimerHandle ReloadTimer;
	FTimerHandle IdleAnimTimer;
	FTimerHandle MuzzleFlashTimer;
	int32 LastFireSoundIndex = INDEX_NONE;
	TWeakObjectPtr<class UAudioComponent> LastFireAudio;

	/** Body mesh the third-person copy is attached to; also a tick prerequisite so the hand has moved first. */
	TWeakObjectPtr<USkeletalMeshComponent> ThirdPersonBody;

	/**
	 * While the hands are on the weapon (hand IK) it is placed from the head along the aim, and this records where it is
	 * relative to the right hand. While an action (reload, equip, fire) takes the hands, it rides the right hand at
	 * that offset, so it follows the animation; the two blend by the hand IK weight.
	 */
	FTransform ThirdPersonHandOffset;
	bool bHasThirdPersonHandOffset = false;

	/** World time the third-person weapon appears, part-way through the body's equip animation. */
	double ThirdPersonRevealTime = 0.0;

	/** The weapon rig's finger bones (right, left; thumb to pinky), found once per mesh. */
	TArray<FName> ThirdPersonRigFingers[2][5];
	TWeakObjectPtr<const USkeletalMesh> ThirdPersonRigFingersMesh;
	/** The body's right-hand finger bones, for placing the weapon in that hand. */
	TArray<FName> ThirdPersonBodyFingers[5];
	TWeakObjectPtr<const USkeletalMesh> ThirdPersonBodyFingersMesh;

	/** The skeletal mesh the weapon light's beam is attached to (the copy this machine sees). */
	TWeakObjectPtr<USceneComponent> WeaponLightBeamParent;

	/** This weapon's casings still in the world, oldest first, so the count can be capped. */
	TArray<TWeakObjectPtr<class ATHShellCasing>> ShellCasings;

	/** MuzzleFlashMaterial with its brightness scaled for th.MuzzleFlashQuality. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> MuzzleFlashMaterial;
};

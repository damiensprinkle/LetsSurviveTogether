#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/THWeaponTypes.h"
#include "THWeaponBase.generated.h"

class APawn;
class UAnimSequenceBase;
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
	void StartFire();
	void StopFire();
	void StartReload();
	void CycleFireMode();

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

private:
	APawn* GetOwningPawn() const;
	bool IsLocallyControlledOwner() const;
	bool IsAimingDownSights() const;

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
	void PlayWeaponAnimation(UAnimSequenceBase* Animation);

	void SetupThirdPersonMesh();
	void AttachThirdPersonMesh();
	void UpdateThirdPersonMesh();

	/** Direction cosmetics use (third-person weapon, muzzle flash, casings). */
	static FRotator GetPresentationAim(const APawn* Pawn);

	/** The copy this machine shows: first person for the owner, third person for everyone else. */
	USkeletalMeshComponent* GetVisibleMesh() const;

	/** Upper-body animation on the owner's body, from the weapon data's BodyAnimSet. */
	void PlayBodyAction(class UAnimSequence* Animation, float Duration, float BlendIn, float BlendOut);
	void PlayFireEffects();
	void ShowMuzzleFlash(const FTransform& Muzzle);
	void EjectShellCasing(const FTransform& Muzzle);
	void PlayImpactEffects(const FVector& Location, const FVector& Normal);

	void ProcessServerShot(const FVector& ViewLocation, const FRotator& ViewRotation, int32 ClientSprayIndex, int32 ShotNumber);
	void FinishReload();

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize10 ViewLocation, FRotator ViewRotation, int32 ClientSprayIndex, int32 ShotNumber);

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerSetFireModeIndex(uint8 NewIndex);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotEffects(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal, bool bHit);

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

	/** This weapon's casings still in the world, oldest first, so the count can be capped. */
	TArray<TWeakObjectPtr<class ATHShellCasing>> ShellCasings;

	/** MuzzleFlashMaterial with its brightness scaled for th.MuzzleFlashQuality. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> MuzzleFlashMaterial;
};

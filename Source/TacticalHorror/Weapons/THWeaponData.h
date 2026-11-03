#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapons/THWeaponTypes.h"
#include "THWeaponData.generated.h"

class UAnimSequenceBase;
class UCameraShakeBase;
class UDamageType;
class UMaterialInterface;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UStaticMesh;

/**
 * Tuning for one weapon. Behaviour lives in ATHWeaponBase; a new weapon should only need
 * a new one of these plus a Blueprint subclass for mesh and animation.
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Cumulative recoil offset (X = yaw right, Y = pitch up, degrees) after ShotIndex shots of a spray. */
	FVector2D GetCumulativeRecoil(int32 ShotIndex) const;

	/** Seconds between shots. */
	float GetFireInterval() const { return 60.0f / FMath::Max(RoundsPerMinute, 1.0f); }

	// Identity

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	ETHWeaponSlot Slot = ETHWeaponSlot::Primary;

	// Damage

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0))
	float Damage = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 1))
	float HeadshotMultiplier = 4.0f;

	/** Bones that count as a headshot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TArray<FName> HeadBoneNames = { TEXT("head") };

	/** Damage multiplier applied per 10 m travelled (CS-style range modifier). 1 = no falloff. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0, ClampMax = 1))
	float RangeModifierPer10m = 0.98f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 100, Units = "cm"))
	float MaxRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UDamageType> DamageType;

	// Firing

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = 1))
	float RoundsPerMinute = 666.0f;

	/** First entry is the default. Toggling cycles through the rest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing")
	TArray<ETHFireMode> FireModes = { ETHFireMode::FullAuto, ETHFireMode::SemiAuto };

	// Ammunition

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 1))
	int32 MagazineSize = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 0))
	int32 StartingReserveAmmo = 90;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 0, Units = "s"))
	float ReloadDuration = 2.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 0, Units = "s"))
	float EquipDuration = 0.5f;

	// Accuracy (degrees of cone half-angle)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0))
	float StandingInaccuracy = 0.15f;

	/** Inaccuracy when moving at full walk speed. Scales linearly with speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0))
	float MovingInaccuracy = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0))
	float AirInaccuracy = 8.0f;

	/** Added per shot in a spray, up to MaxSprayInaccuracy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0))
	float InaccuracyPerShot = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0))
	float MaxSprayInaccuracy = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0, ClampMax = 1))
	float CrouchInaccuracyMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = 0, ClampMax = 1))
	float AimInaccuracyMultiplier = 0.5f;

	/** The first shot of a spray is perfectly accurate when standing still on the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	bool bFirstShotAccurate = true;

	// Recoil

	/**
	 * Per-shot recoil steps in degrees (X = yaw right, Y = pitch up). Entry N is the kick added by shot N.
	 * Shots past the end repeat the last entry. Learnable patterns, not random noise.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	TArray<FVector2D> RecoilPattern;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = 0))
	float RecoilScale = 1.0f;

	/** How much of the recoil moves the camera; the rest only moves the bullets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = 0, ClampMax = 1))
	float ViewKickFraction = 0.5f;

	/** Time without firing after which the next shot starts the pattern again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = 0, Units = "s"))
	float RecoilResetDelay = 0.4f;

	/** Degrees per second the camera returns toward its pre-spray aim after firing stops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = 0))
	float ViewRecoveryRate = 12.0f;

	// Handling

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = 0.1, ClampMax = 1))
	float MoveSpeedMultiplier = 0.95f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = 0.1, ClampMax = 1))
	float AimMoveSpeedMultiplier = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = 5, ClampMax = 170, Units = "deg"))
	float AimFieldOfView = 60.0f;

	/** Seconds to fully aim down sights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = 0.01, Units = "s"))
	float AimTime = 0.15f;

	/** Placeholder hold position relative to the owner's camera, until arms and hand sockets are set up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling")
	FTransform CameraHoldOffset = FTransform(FRotator::ZeroRotator, FVector(30.0f, 12.0f, -14.0f));

	// Animation (played on the weapon mesh, which may include first-person arms)

	/** Looped whenever no other weapon animation is playing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> EquipAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> ReloadAnimation;

	/** Used instead of ReloadAnimation when the magazine was empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> ReloadEmptyAnimation;

	// Third person: what other players see. A copy of the weapon mesh in the body's hand, arms hidden.

	/** Body animations while this weapon is held (pose, aim, fire, reload, equip). Empty = the unarmed set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	TObjectPtr<class UTHBodyAnimSet> BodyAnimSet;

	/** Body bone the weapon is placed from. The head, so the stock stays at the shoulder and the sights at the eye. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FName ThirdPersonAnchorBone = TEXT("head");

	/** Where the weapon's grip bone (the right hand's spot) sits from the anchor bone, in aim space: X forward, Y right, Z up. Moves the whole weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person", meta = (Units = "cm"))
	FVector ThirdPersonAnchorOffset = FVector(18.0f, 5.0f, -12.0f);

	/** Body bone the weapon follows while the hands are busy (reload, equip). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FName ThirdPersonHandBone = TEXT("hand_r");

	/** Bone on the weapon mesh where the body's right hand goes. For first-person rigs with arms: their right hand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FName ThirdPersonGripBone = TEXT("righthand");

	/** Bone on the weapon mesh where the body's left hand goes. For first-person rigs with arms: their left hand. None = left hand not on the weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FName ThirdPersonLeftHandBone = TEXT("lefthand");

	/**
	 * The left hand keeps the body animation's place relative to the right hand instead of going to ThirdPersonLeftHandBone.
	 * For weapons whose body animation already has a good two-handed grip (pistols); ThirdPersonLeftHandOffset nudges it from there.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	bool bThirdPersonLeftHandFollowsRight = false;

	/**
	 * The body's hands copy this weapon's first-person hands: palm placed and turned to match, each finger pointing the
	 * same way. Finds the finger bones by name (thumb / index / middle / ring / pinky) below ThirdPersonGripBone and
	 * ThirdPersonLeftHandBone. The hand offsets and rotations below then nudge from there. Off: hands keep the
	 * body animation's angle and fingers.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	bool bThirdPersonMatchRigHands = true;

	/**
	 * Moves the right / left shoulder (by turning the clavicle), in aim space: X forward, Y right, Z up. The hand stays
	 * on the gun, so a shoulder moved toward it bends that elbow more (X +5..+10 for a rolled-forward support shoulder).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person", meta = (Units = "cm"))
	FVector ThirdPersonRightShoulderOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person", meta = (Units = "cm"))
	FVector ThirdPersonLeftShoulderOffset = FVector::ZeroVector;

	/** Nudges the right hand from the grip bone, in aim space: X forward, Y right, Z up. The weapon stays put. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person", meta = (Units = "cm"))
	FVector ThirdPersonRightHandOffset = FVector::ZeroVector;

	/** Nudges the left hand from its bone, in aim space: X forward, Y right, Z up. The weapon stays put. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person", meta = (Units = "cm"))
	FVector ThirdPersonLeftHandOffset = FVector::ZeroVector;

	/** Turns the right hand from the animation's angle, about the aim axes: pitch tips the fingers up, yaw turns them right, roll turns about the barrel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FRotator ThirdPersonRightHandRotation = FRotator::ZeroRotator;

	/** Turns the left hand from the animation's angle, about the aim axes, as ThirdPersonRightHandRotation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FRotator ThirdPersonLeftHandRotation = FRotator::ZeroRotator;

	/** The weapon points where the player aims; this tilts it from there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FRotator ThirdPersonRotationOffset = FRotator::ZeroRotator;

	/** Material slots hidden on the third-person copy, e.g. the first-person arms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	TArray<FName> ThirdPersonHiddenMaterialSlots = { TEXT("Hand_D"), TEXT("Glove_D") };

	/** Bones (and everything below them) hidden on the third-person copy, e.g. the spare magazine a first-person rig parks beside the gun. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	TArray<FName> ThirdPersonHiddenBones = { TEXT("mag2_j"), TEXT("mag2") };

	/** The third-person copy holds the first frame of this. Empty = IdleAnimation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	TObjectPtr<UAnimSequenceBase> ThirdPersonPose;

	// Presentation

	/** Socket or bone the muzzle is measured from. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Offset from MuzzleSocketName in its local space, for meshes without a muzzle socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FVector MuzzleOffset = FVector::ZeroVector;

	/** Brief light at the muzzle on each shot. In dark levels this is most of the muzzle flash. 0 = off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = 0, Units = "cd"))
	float MuzzleLightIntensity = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = 0, Units = "s"))
	float MuzzleLightDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FLinearColor MuzzleLightColor = FLinearColor(1.0f, 0.65f, 0.3f);

	/** Additive material flashed on crossed quads at the muzzle for MuzzleLightDuration. Empty = no visible flash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UMaterialInterface> MuzzleFlashMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = 0, Units = "cm"))
	float MuzzleFlashSize = 16.0f;

	/** Deferred decal left where a shot hits the world, e.g. a bullet hole. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UMaterialInterface> ImpactDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = 0, Units = "cm"))
	float ImpactDecalSize = 5.0f;

	/** Seconds before an impact decal is removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = 0, Units = "s"))
	float ImpactDecalLifetime = 60.0f;

	/** One is picked at random per shot, never the same twice in a row. Only list takes of the same gun: different guns read as random. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TArray<TObjectPtr<USoundBase>> FireSounds;

	/** Random pitch per shot, picked from this range. Keep it within a couple of percent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FVector2D FireSoundPitchRange = FVector2D(0.985f, 1.015f);

	/** When the next shot fires, the previous shot's tail fades out over this long so bursts don't pile up. 0 = tails overlap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = 0, Units = "s"))
	float FireSoundTailFadeOut = 0.08f;

	/** Caps how many shot tails ring at once; without it full-auto smears into a drone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<USoundConcurrency> FireSoundConcurrency;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<USoundBase> DryFireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> MuzzleFlash;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	// Shell casings: cosmetic physics props spawned locally on every machine that sees the shot

	/** Empty = no casings. Needs simple collision to bounce. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing")
	TObjectPtr<UStaticMesh> ShellMesh;

	/** Rotates the mesh so its length points down the barrel (+X). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing")
	FRotator ShellMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing", meta = (ClampMin = 0.01))
	float ShellScale = 1.0f;

	/** Ejection port relative to the muzzle, in aim space: X forward, Y right, Z up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing", meta = (Units = "cm"))
	FVector ShellEjectOffset = FVector(-8.0f, 1.0f, 1.0f);

	/** Launch velocity in aim space (X forward, Y right, Z up), varied a little per shot. The shooter's velocity is added. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing", meta = (Units = "cm/s"))
	FVector ShellEjectVelocity = FVector(-40.0f, 220.0f, 160.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing", meta = (ClampMin = 0, Units = "s"))
	float ShellLifetime = 10.0f;

	/** Oldest casings from this weapon are removed beyond this many. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing", meta = (ClampMin = 1))
	int32 MaxShellCasings = 30;

	/** One is picked at random when a casing first hits the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing")
	TArray<TObjectPtr<USoundBase>> ShellBounceSounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing", meta = (ClampMin = 0))
	float ShellBounceVolume = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell Casing")
	TObjectPtr<USoundAttenuation> ShellBounceAttenuation;
};

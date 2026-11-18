#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "THEnemyAppearance.generated.h"

class USoundBase;

class UGroomAsset;
class UGroomBindingAsset;
class UIKRetargeter;
class UMaterialInterface;
class UPhysicsAsset;
class USkeletalMesh;

/** One material slot (by name) and the materials it can get, one picked at random. Slots a mesh doesn't have are skipped. */
USTRUCT(BlueprintType)
struct FTHMaterialChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FName Slot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TObjectPtr<UMaterialInterface>> Materials;
};

/**
 * One way to fill an outfit part: these meshes together (a cheerleader top and skirt; empty = nothing, e.g. no shirt),
 * with one pick per material slot shared by all of them, so the pieces match.
 */
USTRUCT(BlueprintType)
struct FTHOutfitChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TObjectPtr<USkeletalMesh>> Meshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHMaterialChoice> Materials;
};

/** A part of the outfit (top, trousers, hair): one of its choices is picked. */
USTRUCT(BlueprintType)
struct FTHOutfitPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHOutfitChoice> Choices;
};

/** Hair, eyebrows or a beard (a MetaHuman's grooms): worn by the outfit piece its binding was made for (the face). */
USTRUCT(BlueprintType)
struct FTHGroom
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UGroomAsset> Groom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UGroomBindingAsset> Binding;

	/**
	 * Cards (and the helmet far off) only, never strands. On: MetaHumans assembled Optimized keep no strands, yet their
	 * closest LODs ask for them, so the hair vanished up close (developer, 2026-11-12). Off only for a groom with strands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseCards = true;
};

/** A limb that can be shot off (developer, 2026-11-13: dismemberment). */
UENUM(BlueprintType)
enum class ETHLimb : uint8
{
	Head,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,
	None UMETA(Hidden)
};

/** The piece that falls off when a limb is shot off: the pack's severed-limb mesh, on the body's skeleton. */
USTRUCT(BlueprintType)
struct FTHSeverableLimb
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	ETHLimb Limb = ETHLimb::LeftArm;

	/** One per body of the look, in the same order (wrapping round when fewer): the piece that matches that body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TObjectPtr<USkeletalMesh>> Meshes;

	/** Its physics (only that limb's bodies); none = the mesh's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UPhysicsAsset> Physics;
};

/** One kind of zombie: its body meshes, their material choices (skin, gore, heads) and outfit parts. */
USTRUCT(BlueprintType)
struct FTHEnemyLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FName Name;

	/** How often this look comes up, relative to the others. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = 0))
	float Weight = 1.0f;

	/** One picked at random. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TObjectPtr<USkeletalMesh>> Bodies;

	/** For bodies without a suitable physics asset of their own; none = the body's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UPhysicsAsset> Physics;

	/** On the body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHMaterialChoice> Materials;

	/** Clothing and hair worn over the body (separate meshes following its animation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHOutfitPart> Parts;

	/** Hair cards and eyebrows (MetaHumans), all of them worn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHGroom> Grooms;

	/** Limbs that can be shot off, and the pieces that fall (a limb not listed stays on). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHSeverableLimb> Limbs;

	const FTHSeverableLimb* FindLimb(ETHLimb Limb) const { return Limbs.FindByPredicate([Limb](const FTHSeverableLimb& Each) { return Each.Limb == Limb; }); }

	/**
	 * Bodies on another skeleton than the enemy's animations (MetaHumans): the animations play on Animated Body (on the
	 * animations' skeleton), hidden but still taking the shots and falling as the ragdoll (with Physics), and this
	 * retargeter copies its pose onto the body you see, every frame. Both set, or neither.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Retarget")
	TObjectPtr<UIKRetargeter> Retargeter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Retarget")
	TObjectPtr<USkeletalMesh> AnimatedBody;

	bool IsRetargeted() const { return Retargeter && AnimatedBody; }
};

/**
 * Random looks for enemies: set on an enemy (Appearance), each one picks a look, a body, materials and clothing at
 * random, different for each enemy and the same on every machine (ATHEnemyCharacter::AppearanceSeed). Made by
 * Scripts/Tools/setup_enemies.py (DA_ZombieLooks: Yarrawah Interactive's M04, M02 and F02 with all their clothing,
 * colour and gore variants).
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHEnemyAppearance : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FTHEnemyLook> Looks;

	/** These bodies' own footsteps, whatever the floor (a mutant charger's stomp; developer, 2026-11-14), one at random per
	 *  step; empty = the body's footstep sounds by surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	/** How loud those footsteps play (developer, 2026-11-15 and 16: the charger's stomp was too loud, then still a bit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = 0, ClampMax = 2))
	float FootstepVolume = 0.3f;

	/** And their pitch: above 1 lighter, below 1 heavier (developer, 2026-11-16: the charger sounded heavier than it looks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = 0.5, ClampMax = 2))
	float FootstepPitch = 1.0f;
};

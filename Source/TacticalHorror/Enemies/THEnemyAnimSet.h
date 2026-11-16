#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "THEnemyAnimSet.generated.h"

class UAnimSequence;

USTRUCT(BlueprintType)
struct FTHEnemyAttack
{
	GENERATED_BODY()

	/** In place (no root motion), played once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimSequence> Animation;

	/**
	 * Where in the animation the attack starts and ends (s; End 0 = the end): one swing out of a loop of several.
	 * Start on the wind-up, so players see it coming.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0, Units = "s"))
	float StartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0, Units = "s"))
	float EndTime = 0.0f;

	/** When the blow lands (s into the animation, between Start and End): the player in reach takes the damage then. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0, Units = "s"))
	float HitTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0.1))
	float PlayRate = 1.0f;

	/** Seconds the attack lasts (Start to End at PlayRate). */
	float GetDuration() const;

	/** Seconds from the attack's start to its blow. */
	float GetHitDelay() const;

	float GetEnd() const;
};

/** Where a shot came from, seen from the enemy. */
UENUM(BlueprintType)
enum class ETHHitSide : uint8
{
	Front,
	Back,
	Left,
	Right
};

/** A stagger clip: which side it reacts to, and whether it is a heavy one (a headshot, a big hit). */
USTRUCT(BlueprintType)
struct FTHEnemyHitReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	FTHEnemyAttack Clip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	ETHHitSide Side = ETHHitSide::Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	bool bHeavy = false;
};

/** One way of lying in wait (a siren) and getting up from it. */
USTRUCT(BlueprintType)
struct FTHEnemyLyingPose
{
	GENERATED_BODY()

	/** Getting up from this pose; with no Lying clip, held on its start frame is how it lies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying")
	FTHEnemyAttack GetUp;

	/** Lying still, looped; none = Get Up held on its start frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying")
	TObjectPtr<UAnimSequence> Lying;

	/** Lying held still on this frame of Lying (s; a death clip's end: a body playing dead) instead of looping it. Below 0 = it loops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying")
	float LyingHoldTime = -1.0f;

	bool HoldsLying() const { return Lying && LyingHoldTime >= 0.0f; }

	/** A player near (a siren beckoning them: a wave, a reach), looped, blended in; none = it just keeps lying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying")
	TObjectPtr<UAnimSequence> Reacting;

	/** Lying and Reacting lowered this much: Mixamo's "Laying" clips lie on a bed, about 96 cm up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying", meta = (Units = "cm"))
	float LyingLowerBy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying", meta = (Units = "cm"))
	float ReactingLowerBy = 0.0f;

	/** Only for playing dead (a siren shot down drops into it); never picked for waiting. With none such, playing dead picks
	 *  from all. (Developer, 2026-11-14: a siren waits standing or sitting, never lying.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying")
	bool bPlayingDeadOnly = false;
};

/**
 * The animations an enemy body plays (UTHEnemyAnimInstance): idle, walk and run loops blended by speed, and attacks.
 * All in place: the capsule moves the body. Per kind of body (standing, crawling); made by setup_enemies.py.
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHEnemyAnimSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Walk;

	/** How fast the walk's feet move at play rate 1 (measured from its root-motion twin); it plays faster or slower to match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 1, Units = "cm/s"))
	float WalkAnimSpeed = 100.0f;

	/** A walk that isn't in place (Mixamo's Zombie Crawl moves its pelvis 2 m per loop): how far its pelvis goes in one loop, in
	 *  the mesh's space; it is held in place by taking that back as it plays. Zero = in place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (Units = "cm"))
	FVector WalkTravel = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Run;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 1, Units = "cm/s"))
	float RunAnimSpeed = 300.0f;

	/** As Walk Travel, for a run that isn't in place (Mixamo's Mutant Run goes 194 cm a loop). Zero = in place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (Units = "cm"))
	FVector RunTravel = FVector::ZeroVector;

	/** The run plays at most this much faster than recorded; above that the feet slide instead of looking frantic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 1, ClampMax = 4))
	float MaxRunPlayRate = 2.5f;

	/** Walking calmly like a person (a hunter pacing); none = the Walk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CalmWalk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 1, Units = "cm/s"))
	float CalmWalkAnimSpeed = 150.0f;

	/** Crouched and still (a stalker hiding behind low cover); none = it doesn't crouch. Only while not moving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> Crouch;

	/** Moving crouched (a stalker creeping); none = it stands up to move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UAnimSequence> CrouchWalk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = 1, Units = "cm/s"))
	float CrouchWalkAnimSpeed = 150.0f;

	/** A charger's roar before it charges (a window of a clip, like an attack; its hit time unused). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger")
	FTHEnemyAttack Roar;

	/** A charger that missed: it goes down (Stumble Fall), stays down (Stumble Down, looped), then gets up (Stumble Get Up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger")
	FTHEnemyAttack StumbleFall;

	/** Looped on the ground. The Stumble Get Up clip itself: held on that window's start frame instead (a siren lying there). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger")
	TObjectPtr<UAnimSequence> StumbleDown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger")
	FTHEnemyAttack StumbleGetUp;

	/**
	 * Lying in wait (a siren): one picked at random each time it lies down, on its back or its stomach (developer,
	 * 2026-11-13: always on its back looked set up). Empty = Stumble Down, then Stumble Get Up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lying")
	TArray<FTHEnemyLyingPose> LyingPoses;

	/** Seconds a stumble takes with DownTime spent on the ground: fall, down, get up. */
	float GetStumbleDuration(float DownTime) const { return StumbleFall.GetDuration() + DownTime + StumbleGetUp.GetDuration(); }

	/** One picked at random per attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TArray<FTHEnemyAttack> Attacks;

	/** With both arms shot off: one of these instead (a bite; developer, 2026-11-16). Empty = it keeps swinging its stumps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TArray<FTHEnemyAttack> ArmlessAttacks;

	/** Attacks then Armless Attacks, by one index (as replicated); null past both. */
	const FTHEnemyAttack* GetAttack(int32 Index) const
	{
		return Attacks.IsValidIndex(Index) ? &Attacks[Index]
			: ArmlessAttacks.IsValidIndex(Index - Attacks.Num()) ? &ArmlessAttacks[Index - Attacks.Num()] : nullptr;
	}

	/**
	 * Shot (its behaviour's Stagger Time): played over the body, one at random of those for the side the shot came from
	 * and its weight (heavy: a headshot or a big hit), else any for that side, else any (developer, 2026-11-13).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TArray<FTHEnemyHitReaction> HitReactions;

	/** A leg shot off: it plays this set instead (crawling). None = it keeps standing (limping). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TObjectPtr<UTHEnemyAnimSet> LegsLost;

	/** Index into Hit Reactions for a shot from Side (heavy or not); INDEX_NONE if there are none. */
	int32 PickHitReaction(ETHHitSide Side, bool bHeavy) const;
};

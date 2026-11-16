#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/Components/THHealthComponent.h"
#include "Enemies/THEnemyAnimSet.h"
#include "Enemies/THEnemyAppearance.h"
#include "THEnemyCharacter.generated.h"

class UPhysicalMaterial;
class UTHEnemyAnimSet;
class UTHEnemyAppearance;
class UTHEnemyBehavior;
class UTHFootstepComponent;
class UPhysicsAsset;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;

/** How an enemy died, replicated so every machine drops the same body the same way (and late joiners see a corpse). */
USTRUCT()
struct FTHEnemyDeath
{
	GENERATED_BODY()

	UPROPERTY()
	bool bDead = false;

	/** The killing shot's direction (zero if it wasn't a shot). */
	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ZeroVector;

	/** The bone the killing shot hit. */
	UPROPERTY()
	FName Bone;
};

/** An attack as it starts, replicated so every machine plays the same animation (and the server's bones match). */
USTRUCT()
struct FTHEnemyAttackEvent
{
	GENERATED_BODY()

	/** Goes up by one per attack, so the same attack twice in a row still replicates. */
	UPROPERTY()
	uint8 Count = 0;

	/** Into the body's UTHEnemyAnimSet::Attacks. */
	UPROPERTY()
	uint8 Index = 0;
};

/** A charger's roar or stumble, replicated so every machine plays it. */
USTRUCT()
struct FTHEnemyActionEvent
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Count = 0;

	/** 1 roar, 2 stumble, 3 lie down (DownTime > 0: dropping into it), 4 get up, 5 stagger (Pose: which of the set's Hit
	 *  Reactions), 6 stop lying (to crawl), 7 up from crawling (Pose: the lying pose to rise from). */
	UPROPERTY()
	uint8 Kind = 0;

	/** A stumble's time on the ground; a stagger's time stopped. */
	UPROPERTY()
	float DownTime = 0.0f;

	/** Lying down: which of the body set's Lying Poses. */
	UPROPERTY()
	uint8 Pose = 0;
};

/** How a placed enemy waits for players (its Wait Pose). */
UENUM(BlueprintType)
enum class ETHWaitPose : uint8
{
	AsBehavior UMETA(DisplayName = "As Behaviour"),
	Lying,
	Standing
};

/**
 * An enemy: server-side health (no downed state), hit per bone by bullets through its physics asset, bleeds (its bones
 * use the Flesh physical material, see UTHImpactSurfaces), drops as a ragdoll when killed. What it does is its Behavior
 * (hunt, stalk, ambush, or nothing), run on the server by ATHEnemyAIController; it moves on the navigation mesh and
 * attacks players in reach. Animation: UTHEnemyAnimInstance from its Animations set.
 * Blueprints and assets: Scripts/Tools/setup_enemies.py.
 */
UCLASS(Blueprintable)
class TACTICALHORROR_API ATHEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATHEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UTHHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return Death.bDead; }

	/** Whether this machine has dropped the body as a ragdoll. */
	bool IsRagdoll() const { return bRagdoll; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UTHEnemyBehavior* GetBehavior() const { return Behavior; }

	/** Server: changes what it does from now on (null = stands still). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy")
	void SetBehavior(UTHEnemyBehavior* NewBehavior);

	/** The behaviour's own animations if it has some (a siren: a person's), else the body's. Replicated. */
	UTHEnemyAnimSet* GetAnimations() const { return AnimationsInUse ? AnimationsInUse.Get() : Animations.Get(); }

	/**
	 * Server: starts an attack on Target (a random one of the body's attacks). The blow lands at the attack's HitTime
	 * if Target is still in reach and in front: the behaviour's AttackDamage. False if it can't attack now.
	 */
	bool StartAttack(AActor* Target);

	/** Server: a blow on a closed door at Point (one of its attack swings, the behaviour's Door Bang Sounds and a little
	 *  shake as it lands, on every machine). Returns the swing's length (s); 0 if it can't now. */
	float BangOnDoor(const FVector& Point, const FVector& Normal);

	/** Server: a crash at Point with Sound and the behaviour's wall-crash shake, on every machine (a charger slamming a door). */
	void PlayCrashAt(const FVector& Point, const FVector& Normal, USoundBase* Sound);

	/** Server: a charger's roar (the body set's Roar), on every machine. */
	void PlayRoar();

	/** Server: a charger's stumble (fall, DownTime on the ground, get up), on every machine. Returns its whole length (s). */
	float PlayStumble(float DownTime);

	/** Server: a stagger when shot from Side (heavy: a headshot or a big hit), one of the body set's Hit Reactions for it, on
	 *  every machine. */
	void PlayHitReaction(ETHHitSide Side, bool bHeavy, float StoppedFor = 0.0f);

	/** Server: where the latest shot came from (seen from it), and whether it was heavy (a headshot, or a quarter of its health). */
	ETHHitSide GetLastHitSide() const { return LastHitSide; }
	bool WasLastHitHeavy() const { return bLastHitHeavy; }

	/**
	 * Wounds (developer, 2026-11-13: hits that matter). A limb whose bones took its behaviour's Limb Health comes off (on
	 * every machine: hidden on the body and its clothes, a wound on skins that have one, the look's piece falling away, a
	 * spray of blood); a leg lost and it crawls (its body set's Legs Lost), an arm lost and its blows are weaker, a leg hurt
	 * and it limps. The head only comes off with a strong killing headshot.
	 */
	bool IsLimbSevered(ETHLimb Limb) const { return Limb != ETHLimb::None && (SeveredLimbs & (1 << static_cast<uint8>(Limb))) != 0; }

	/** Its look has a piece for Limb (only those come off). */
	bool CanSever(ETHLimb Limb) const;

	/** Server: Limb comes off now, flying along Direction (normally a shot does it). False if it can't (or already has). */
	bool SeverLimb(ETHLimb Limb, const FVector& Direction);

	/** Lost a leg and crawls (its body set's Legs Lost). */
	bool IsCrawling() const { return bCrawling; }

	/** Server: its speeds times this (a leg hurt: limping). */
	float GetSpeedScale() const;

	/** Server: its blows' damage times this (arms lost). */
	float GetAttackDamageScale() const;

	/** Server: damage taken by Limb so far. */
	float GetLimbDamage(ETHLimb Limb) const { return Limb == ETHLimb::None ? 0.0f : LimbDamage[static_cast<uint8>(Limb)]; }

	/** Which limb a bone belongs to (its own or a parent's: head and neck, upper arm and below, thigh and below). */
	ETHLimb LimbOfBone(FName Bone) const;

	/** Every machine: the pieces shot off so far (tests). */
	int32 GetSeveredPieceCount() const { return SeveredPieces.Num(); }
	AActor* GetSeveredPiece(int32 Index) const { return SeveredPieces.IsValidIndex(Index) ? SeveredPieces[Index].Get() : nullptr; }

	/** Its running steps (tests). */
	const UTHFootstepComponent* GetFootsteps() const { return FootstepComponent; }

	/** Every machine: how many of the visible body's bones are drawn collapsed, for limbs off (tests). */
	int32 GetCollapsedBoneCount() const { return CollapsedBones.Num(); }

	/** Gives a piece that came off the materials the body (or its clothes) has in the slots of the same name. */
	static void MatchPieceMaterials(USkeletalMeshComponent* Piece, const USkeletalMeshComponent* Body,
		TConstArrayView<TObjectPtr<USkeletalMeshComponent>> Clothes);

	/** Server: lies on the ground (one of the body set's Lying Poses, ForcedPose or at random; else Stumble Down) until GetUp, on
	 *  every machine. bBlend: it drops into it (a collapse) rather than already lying there. bPlayingDead: a pose kept for
	 *  playing dead (Playing Dead Only); otherwise one that isn't (a siren's are standing and sitting: "lying" in wait). */
	void LieDown(int32 ForcedPose = INDEX_NONE, bool bBlend = false, bool bPlayingDead = false);

	/** Server: from lying (or standing), down on its belly crawling with CrawlSet (a siren crawling to someone). */
	void StartCrawling(UTHEnemyAnimSet* CrawlSet);

	/** Server: up from crawling (back to its own animations; a get-up from face down). Returns how long that takes (s). */
	float RiseFromCrawl();

	/** Server: gets up from lying, on every machine. Returns how long that takes (s). */
	float GetUp();

	/** Server: lying on the ground (LieDown, not yet GetUp). */
	bool IsLyingDown() const { return bLyingDown; }

	/** Which of the body set's Lying Poses it lies in (or last lay in); -1 = none (Stumble Down). */
	int32 GetLyingPose() const;

	/** Server: lying, it beckons a player near it (the pose's Reacting clip). Replicated. */
	void SetBeckoning(bool bNow);
	bool IsBeckoning() const { return bBeckoning; }

	/** Players' lights within this of it flicker (0: none). Replicated. */
	float GetLightFlickerDistance() const { return IsDead() ? 0.0f : LightFlickerDistance; }

	/** Server: a blow that isn't an attack animation's (a charge): Damage to Target, the enemy's controller as instigator. */
	void DealBlow(AActor* Target, float Damage);

	/**
	 * Server: a charger ran into a wall at Point (Normal: out of the wall). On every machine: its behaviour's crash effect
	 * and sound there, and the view of each local player nearby shakes (WallCrashShake, fading out to WallCrashShakeRadius).
	 */
	void WallCrash(const FVector& Point, const FVector& Normal);

	/** Server: how many times it has run into a wall (tests). */
	int32 GetWallCrashCount() const { return WallCrashCount; }

	/** Server: how many blows it has landed on doors (tests). */
	int32 GetDoorBangCount() const { return DoorBangCount; }

	/** Server: an attack is playing (from its start to its animation's end). */
	bool IsAttacking() const;

	/** Server: when the last attack ended (world time). */
	double GetLastAttackEndTime() const { return AttackEndTime; }

	/** Server: whether Target is within the behaviour's AttackRange (capsule edge to capsule edge), roughly in front, and
	 *  nothing in between (HasClearReach). */
	bool IsInReach(const AActor* Target, float RangeScale = 1.0f) const;

	/** Nothing between it and Target (walls, doors, crates, props; not bodies) at chest and waist height, from From (its
	 *  own place by default). No blows through a crate or round a door frame (developer, 2026-11-13). */
	bool HasClearReach(const AActor* Target, const FVector* From = nullptr) const;

	/** Server: turns towards a point at the movement's rotation rate (for standing still: attacking, watching). */
	void TurnTowards(const FVector& Location, float DeltaSeconds);

	/**
	 * Server: plays one of Sounds (picked at random) from the enemy on every machine, with the behaviour's voice
	 * attenuation and volume. One voice at a time: false (nothing played) while the last one is still going.
	 */
	bool PlayVoice(const TArray<TObjectPtr<USoundBase>>& Sounds);

	/** Server: walks like a person (the body set's Calm Walk) instead of its zombie walk. Replicated. */
	void SetCalmWalk(bool bCalm);
	bool IsCalmWalking() const { return bCalmWalk; }

	/** Server: it is going for a player (chasing, attacking, roaring, charging, striking). Replicated: the music speeds up. */
	void SetAggressive(bool bNow);
	bool IsAggressive() const { return bAggressive; }

	/** This enemy's own voice pitch, so a group doesn't sound like one person. */
	float GetVoicePitch() const { return VoicePitch; }

	/** Server: bends its head and neck down this many degrees (an ambusher staring at the floor); 0 = looks ahead. Replicated. */
	void SetHeadDown(float Degrees);
	float GetHeadDown() const { return HeadDown; }

	/** Server: Sound from its feet on every machine (a charger's stomps), apart from its voice. */
	void PlaySoundHere(USoundBase* Sound);

	/** Server: the voice the last PlayVoice started is still going. */
	bool IsVoicePlaying() const;

	/** Every machine: the last voice played here (for tests). */
	USoundBase* GetLastVoice() const { return LastVoice.Get(); }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UTHHealthComponent> HealthComponent;

	/** Its running steps (developer, 2026-11-13: zombies should make footstep noises when they run): sounds per surface
	 *  copied from the player's (setup_enemy_footsteps.py); not a noise enemies hear. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<class UTHFootstepComponent> FootstepComponent;

	/**
	 * What it does: Hunter, Stalker or Ambusher (/Game/Enemies/Behaviors), or empty to just stand there (the line-up,
	 * the tests). Set per placed enemy in the Details panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UTHEnemyBehavior> Behavior;

public:
	/**
	 * How this one waits for players (an ambusher, a siren): as its behaviour says (Lies Down, Lies Down Chance), lying down
	 * (playing dead), or standing (an ambusher head down). An actor tag Lying or Standing does the same, if this is As
	 * Behaviour (developer, 2026-11-14: some ambushers standing, like before).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	ETHWaitPose WaitPose = ETHWaitPose::AsBehavior;

protected:

	/** Idle, walk, run and attacks for this body (standing or crawling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Body")
	TObjectPtr<UTHEnemyAnimSet> Animations;

	/** The body. Set on the Blueprint; applied to the Mesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Body")
	TObjectPtr<USkeletalMesh> BodyMesh;

	/** For a body whose mesh has no suitable physics asset (without one, bullets can't hit it and it can't fall as a ragdoll). Empty: the mesh's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Body")
	TObjectPtr<UPhysicsAsset> PhysicsAssetOverride;

	/**
	 * A random look as it starts playing (body, skin and gore, clothing, hair), different for each enemy and the same on
	 * every machine; replaces Body Mesh. None = Body Mesh as set. BP_Zombie uses DA_ZombieLooks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Body")
	TObjectPtr<class UTHEnemyAppearance> Appearance;

public:
	/** Every machine: the clothing and hair worn now (for tests). */
	const TArray<TObjectPtr<USkeletalMeshComponent>>& GetOutfit() const { return Outfit; }

	/**
	 * The body players see: a retargeted look's own body (a MetaHuman, following the hidden animated mesh), else the
	 * mesh itself.
	 */
	USkeletalMeshComponent* GetVisibleBody() const;

	/** Every machine: hair and eyebrows worn now (for tests). */
	const TArray<TObjectPtr<class UGroomComponent>>& GetGrooms() const { return Grooms; }
	int32 GetAppearanceSeed() const { return AppearanceSeed; }

protected:

	/** On every bone, so shots know they hit flesh (blood, no bullet hole). Surface type Flesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Body")
	TSoftObjectPtr<UPhysicalMaterial> FleshMaterial;

	/** Speed (cm/s) the killing shot gives the bone it hit as the body drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death", meta = (ClampMin = 0, Units = "cm/s"))
	float DeathShotSpeed = 350.0f;

	/** Played from the body when a shot kills it, on every machine, one at random. Not for a headshot kill (the shot's headshot sound already plays). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death")
	TArray<TObjectPtr<USoundBase>> KillSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death")
	TObjectPtr<USoundAttenuation> KillSoundAttenuation;

	/** Bones that count as the head (as the guns' HeadBoneNames). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death")
	TArray<FName> HeadBoneNames = { TEXT("head") };

	/** A limb coming off: blood from the wound, and its sound (one at random). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Wounds")
	TObjectPtr<class UNiagaraSystem> SeverEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Wounds")
	TArray<TObjectPtr<USoundBase>> SeverSounds;

	/** The head coming off: one of these instead of Sever Sounds (empty: Sever Sounds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Wounds")
	TArray<TObjectPtr<USoundBase>> HeadSeverSounds;

	/** Seconds a piece that came off stays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Wounds", meta = (ClampMin = 0, Units = "s"))
	float SeveredPieceLifeSpan = 60.0f;

	/** Seconds a corpse stays, at least (while the Director brings more, bodies would pile up; 2026-11-13); 0 = until the
	 *  level ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death", meta = (ClampMin = 0, Units = "s"))
	float CorpseLifeSpan = 180.0f;

	/** Its time up, a corpse still stays while any player is this close (it never vanishes in front of anyone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death", meta = (ClampMin = 0, Units = "cm"))
	float CorpseKeepDistance = 2500.0f;

private:
	void RemoveCorpse();

	FTimerHandle CorpseTimer;

	/** Whoever hurt it last (its killer, as it dies). */
	TWeakObjectPtr<APawn> LastAttacker;

	void ApplyBody();

	/** Builds the look AppearanceSeed picks from Appearance (every machine, the same). */
	void ApplyAppearance();

	UFUNCTION()
	void OnRep_AppearanceSeed();

	/** Picked by the server as it starts playing; 0 = not yet (or no Appearance). */
	UPROPERTY(ReplicatedUsing = OnRep_AppearanceSeed)
	int32 AppearanceSeed = 0;
	int32 AppliedAppearanceSeed = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> Outfit;

	/** A retargeted look's body (UTHRetargetAnimInstance), on top of the hidden animated mesh; null otherwise. */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> VisibleBody;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UGroomComponent>> Grooms;

	UFUNCTION()
	void HandleHealthStateChanged(UTHHealthComponent* Health, ETHHealthState NewState, ETHHealthState OldState);

	UFUNCTION()
	void OnRep_Death();

	/** Drops the body if it's dead and not yet dropped. bAsItHappens: the death is news (kill sound), not found on joining. */
	void ApplyDeath(bool bAsItHappens);
	void BecomeRagdoll();
	void PlayKillSound();

	UFUNCTION()
	void OnRep_Attack();

	/** Server: the blow of the current attack lands (or misses). */
	void LandAttack();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastVoice(USoundBase* Sound, USoundAttenuation* Attenuation, float Volume);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(USoundBase* Sound, USoundAttenuation* Attenuation);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastWallCrash(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, class UNiagaraSystem* Effect, USoundBase* Sound, float ShakeRadius, float Shake, float EffectTime = 0.0f, float EffectScale = 1.0f);

	UPROPERTY(Replicated)
	float HeadDown = 0.0f;

	UPROPERTY(Replicated)
	bool bCalmWalk = false;

	UPROPERTY(Replicated)
	bool bBeckoning = false;

	/** Players' lights within this flicker (its behaviour's Light Flicker Distance), replicated: each machine flickers its own. */
	UPROPERTY(Replicated)
	float LightFlickerDistance = 0.0f;

	UPROPERTY(Replicated)
	bool bAggressive = false;

	/** Picked by the server as it starts playing (0.88 to 1.12). */
	UPROPERTY(Replicated)
	float VoicePitch = 1.0f;

	double VoiceBusyUntil = -1.0e9;
	TWeakObjectPtr<USoundBase> LastVoice;

	UPROPERTY(ReplicatedUsing = OnRep_Death)
	FTHEnemyDeath Death;

	UPROPERTY(ReplicatedUsing = OnRep_Attack)
	FTHEnemyAttackEvent Attack;

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	FTHEnemyActionEvent Action;

	/** Its behaviour's own animations (a siren: a person's), if any. Replicated. */
	UPROPERTY(Replicated)
	TObjectPtr<UTHEnemyAnimSet> AnimationsInUse;

	bool bLyingDown = false;

	int32 WallCrashCount = 0;
	int32 DoorBangCount = 0;
	FTimerHandle DoorBangTimer;

	UFUNCTION()
	void OnRep_Action();

	/** The looks this enemy is built from: its behaviour's (a charger's mutants) or its own Appearance. Replicated. */
	UPROPERTY(ReplicatedUsing = OnRep_AppearanceSeed)
	TObjectPtr<UTHEnemyAppearance> AppearanceInUse;

	/** Server: the current attack's target, and when it ends. */
	TWeakObjectPtr<AActor> AttackTarget;
	double AttackEndTime = -1.0e6;
	FTimerHandle AttackHitTimer;

	/** Server: the latest shot, which becomes Death when it kills. */
	FTHEnemyDeath LastHit;

	/** Wounds: limbs off (a bit per ETHLimb), replicated; the direction the last went; damage per limb (server). */
	UPROPERTY(ReplicatedUsing = OnRep_SeveredLimbs)
	uint8 SeveredLimbs = 0;
	uint8 AppliedSeveredLimbs = 0;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal SeverDirection = FVector::ZeroVector;

	UPROPERTY(Replicated)
	bool bCrawling = false;

	float LimbDamage[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	ETHHitSide LastHitSide = ETHHitSide::Front;
	bool bLastHitHeavy = false;

	UFUNCTION()
	void OnRep_SeveredLimbs();

	/** Every machine: shows Limb gone (its bones collapsed, wound, the falling piece, blood). */
	void ApplySevered(ETHLimb Limb);

	/**
	 * A limb off: its bones (Top and everything below) drawn shrunk to nothing at Top's joint, each frame once Body's pose is
	 * final. Not the engine's bone hiding, which shrinks them onto the parent's reference origin: a point a body's length
	 * away, so a cut leg's skin and robe stretched into a spike (developer, 2026-11-16).
	 */
	void CollapseLimb(USkeletalMeshComponent* Body, FName Top);
	void CollapseSeveredBones();

	/** Bone -> the top bone of its severed limb, on CollapsedBody. */
	TArray<TPair<int32, int32>> CollapsedBones;
	TWeakObjectPtr<USkeletalMeshComponent> CollapsedBody;
	FDelegateHandle CollapseHandle;

	/** The look and body ApplyAppearance picked (for the pieces that fall). */
	const struct FTHEnemyLook* CurrentLook = nullptr;
	int32 CurrentBodyIndex = 0;

	TArray<TWeakObjectPtr<AActor>> SeveredPieces;

	bool bRagdoll = false;
};

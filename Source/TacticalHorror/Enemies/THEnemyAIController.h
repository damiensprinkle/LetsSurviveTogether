#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "THEnemyAIController.generated.h"

class ATHEnemyCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UTHEnemyBehavior;
class UNavigationPath;
class UTHDoorSyncComponent;

/** What an enemy is doing now (server only; for debugging and the tests). */
UENUM(BlueprintType)
enum class ETHEnemyState : uint8
{
	/** Nothing to do: standing (between wanders, or an enemy with no behaviour). */
	Idle,
	Wander,
	/** Going to a noise it heard, then looking around there. */
	Investigate,
	Chase,
	Attack,
	/** A stalker moving between places a player can't see, keeping its distance behind them. */
	Stalk,
	/** A stalker going to hide (seen, approached, shot, or after a strike). */
	Retreat,
	/** A stalker hidden and still: unaware of anyone yet, or waiting for the player it follows to look away. */
	Lurk,
	/** A stalker going in for a blow. */
	Strike,
	/** An ambusher waiting. */
	Ambush,
	/** A charger roaring at a player, about to charge. */
	Roar,
	/** A charger charging. */
	Charge,
	/** A charger on the ground after a missed charge, then getting up (or recovering after a hit). */
	Stumble,
	/** A siren waiting, lying or standing, calling for help. */
	Siren
};

/**
 * The mind of an ATHEnemyCharacter, on the server. Sight and hearing (AI Perception: players' footsteps and shots are
 * noises, UAISense_Hearing) feed a small state machine that does what the enemy's UTHEnemyBehavior says: hunt, stalk
 * or ambush. Moves on the navigation mesh. No Behavior Tree: the logic is all here, so it's easy to read and test.
 */
UCLASS()
class TACTICALHORROR_API ATHEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATHEnemyAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;
	virtual void StopMovement() override;

	/** Reads the enemy's Behavior again (on possession, and when it changes). Forgets what it knew. */
	void ApplyBehavior();

	/** The enemy was hurt; Attacker (may be null) becomes known. */
	void NotifyDamaged(APawn* Attacker);

	/** The enemy was killed by Attacker (may be null), as it dies (its pawn still possessed). */
	void NotifyKilled(APawn* Attacker);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	ETHEnemyState GetEnemyState() const { return State; }

	/** The player it is after (chasing, stalking), if any. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	APawn* GetTarget() const { return Target.Get(); }

	/**
	 * Whether this player could see the enemy if it stood at FeetLocation: within the behaviour's ViewAngle either side of
	 * where they face (sideways only), with a clear line from their eyes to any part of it (bWholeBody) or to the middle
	 * of its chest or head.
	 */
	bool IsInViewOf(const APawn* Player, const FVector& FeetLocation, bool bCrouched = false, bool bWholeBody = true) const;

	/** How far a noise of Loudness made at NoiseLocation carries to it: its hearing range, cut by Wall Muffle per wall between. */
	float HeardRangeThroughWalls(const FVector& Where, float Loudness) const;

	/** Walls, doors, crates and props (each once) on the line between the two points. */
	int32 CountWallsBetween(const FVector& From, const FVector& To) const;

	/** A flashlight the player carries is shining on it: inside the beam's cone and range, nothing in between. */
	bool IsLitBy(const APawn* Player) const;

	/** A stalker: whether it has noticed a player (heard them, was shot, or one came right up to it) and follows them. */
	bool IsStalking() const { return bHooked; }

	/** The director: it knows where Player is and comes for them (a wave sent in). */
	void Summon(APawn* Player) { HearCall(Player); }

	/** The director's breather: no strikes or lures for this long (a stalker). */
	void HoldOff(float Seconds);

	/** Its own pace and blow rhythm (Variation): its speeds and the time between blows times these. */
	float GetSpeedVariation() const { return SpeedVariation; }
	/** Going round to a player's side or back while another comes on (Flanks). */
	bool IsFlanking() const { return bFlanking; }
	/** A charger: warnings played in its latest roar. */
	int32 GetChargeTellsPlayed() const { return TellsPlayed; }
	/** A stalker: lures played so far (tests). */
	int32 GetLuresPlayed() const { return LuresPlayed; }
	/** Sees Player now (tests). */
	bool CanSee(const APawn* Player) const { return IsVisible(Player); }
	float GetCooldownVariation() const { return CooldownVariation; }

	/** Shot a moment ago: stopped, staggering (Stagger Time). */
	bool IsStaggered() const { return GetWorld() && GetWorld()->GetTimeSeconds() < StaggerUntil; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	struct FKnownPlayer
	{
		/** Where they are (seen now) or were last seen or heard shooting it. */
		FVector Location = FVector::ZeroVector;
		double LastSensed = -1.0e9;
		bool bVisible = false;
		/** Just seen, it reacts no sooner than this (its Reaction Time). */
		double NoticeAt = 0.0;
		/** Their pace as it lost sight of them. */
		FVector Velocity = FVector::ZeroVector;
	};

	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);

	void ThinkHunter(float DeltaSeconds);
	void ThinkStalker(float DeltaSeconds);
	void ThinkAmbusher(float DeltaSeconds);
	void ThinkCharger(float DeltaSeconds);
	void ThinkSiren(float DeltaSeconds);
	/** A charger: roars at Player, then charges. */
	void StartRoar(APawn* Player);
	/** A charger's charge ends: a hit (recovers briefly), a wall crash (stunned) or a miss (pulls up, then fights on). */
	enum class EChargeEnd : uint8 { Hit, Crash, Miss };
	void EndCharge(EChargeEnd How);
	/** Stalker: goes straight for its prey from wherever it is (cornered, or a standoff). */
	void Rush();
	/** A charger: nothing in the way, its body's width, in a straight line to Player. */
	bool HasClearRun(const APawn* Player) const;
	/** Moving (a move request under way) but hardly getting anywhere for a second: pushing against something. */
	bool IsStuck();
	/** A stalker stuck on the way somewhere: forget that place for a while and pick another. */
	void AvoidCurrentGoal();
	/** Near a place it got stuck going to (within the last 10 s). */
	bool IsAvoided(const FVector& Location) const;
	/** A hunter with nothing to do: walks a circle about its spot (PaceRadius), like a person. */
	void Pace();
	/** An ambusher: bursts out at Player (who becomes known). */
	void Spring(APawn* Player);
	/** An ambusher springing: sets off every waiting ambusher within its PackAlertRadius at Player. */
	void AlertPack(APawn* Player);

	/** Hunting tactics (developer, 2026-11-13). Flank: others after Player too and one nearer: it heads for their side or back
	 *  first (true while it does). */
	bool Flank(APawn* Player);
	/** Aimed at (within Weave Angle, 4 to 15 m, a clear line): the next point of its zig-zag at Player (false: straight on). */
	bool FindWeavePoint(const APawn* Player, FVector& OutLocation);
	/** Lost sight of Player: where to look, ahead of Last along their last pace (Search Ahead), on the navigation mesh. */
	FVector SearchPoint(const APawn* Player, const FVector& Last) const;
	/** Starting a chase: others of its kind within Scream Radius come running to Player. */
	void CallOthers(APawn* Player);
	/** Called by another's scream: goes for Player (known there now), running. */
	void HearCall(APawn* Player);
	/** Opens closed doors about Ahead in front of it (Opens Doors; bAlways: whatever its behaviour says, every call: a charge). */
	void OpenDoorsAhead(float Ahead = 120.0f, bool bAlways = false, bool bSlam = false);
	/** Unlocked doors don't count as in the way (a charger bursts through them). */
	void IgnoreOpenableDoors(FCollisionQueryParams& Params) const;
	/** Lit by Player's flashlight from more than 3 m: backs off into the dark a moment (Shuns Light; true while it does). */
	bool ShunLight(APawn* Player);

	/** Runs at Player and attacks when in reach, with nothing in between (else round it to a clear line, or waits). */
	void Engage(APawn* Player, float DeltaSeconds, ETHEnemyState MovingState);
	/** A place just inside its reach of Player with nothing in between, the shortest walk away (false if none). */
	bool FindReachSpot(const APawn* Player, FVector& OutLocation) const;
	/** Goes to the last noise and looks around there; false when there is no noise to follow. */
	bool Investigate(float DeltaSeconds);
	void IdleOrWander();

	/** The player to go after: seen now over remembered, standing over downed, near over far; null if none. */
	APawn* PickTarget();
	bool IsTargetable(const APawn* Player) const;
	FVector GetPlayerLocation(const APawn* Player) const;
	bool IsVisible(const APawn* Player) const;
	/** It can make Player out where they are: lit, or within its Dark Sight Distance (always, without one). */
	bool CanMakeOut(const APawn* Player) const;

	/** Walks or runs there; bCrouched: crouched (if its body can), else standing. */
	void MoveTowards(const FVector& Location, float Speed, float AcceptanceRadius, bool bCrouched = false);
	void MoveTowardsActor(AActor* Goal, float Speed, float AcceptanceRadius);
	/** The last MoveTowards has ended (arrived, or there is no way there). */
	bool HasArrived() const;
	void SetSpeed(float Speed);
	/** Gets up from a crouch (before moving). */
	void StandUp();
	/** Crouches, if its body has a crouch. */
	void CrouchDown();
	bool CanCrouch() const;
	/** Crouched or crouching. */
	bool IsLow() const;
	/** A stalker counts low cover (a crate it crouches behind) as hiding it: Uses Low Cover (off since 2026-11-13). */
	bool CoversCrouched() const;

	/** A place on the navigation mesh about Distance from From, away from Player (preferably where Player can't see it). */
	bool FindPlaceAwayFrom(const APawn* Player, const FVector& From, const FVector& Direction, float Distance, FVector& OutLocation) const;
	/**
	 * A clear line from Player's eyes to its body (standing or crouched) at FeetLocation; bodies don't block. bWholeBody:
	 * any part of it (chest, head, top of the head, either side); otherwise only the middle of its chest or head.
	 */
	bool CanBeSeenFrom(const APawn* Player, const FVector& FeetLocation, bool bCrouched = false, bool bWholeBody = true) const;
	/** A stalker partly in view: a place a few steps away, round its cover, that no player can see any of it (false if none). */
	bool FindCoverNearby(FVector& OutLocation) const;
	/** Whether any of its body at FeetLocation can be seen from From (see CanBeSeenFrom). */
	bool IsAnyPartVisible(const FVector& From, const FVector& FeetLocation, bool bCrouched, bool bWholeBody) const;
	/** Out of Player's sight at FeetLocation from where they are and from a step either side or towards it (real cover). */
	bool IsCoveredFrom(const APawn* Player, const FVector& FeetLocation, bool bCrouched) const;
	/** Walls, doors, crates and props between the two points (not bodies). */
	bool IsBlocked(const FVector& From, const FVector& To) const;
	bool IsInAnyonesView(const FVector& FeetLocation, bool bCrouched = false, bool bWholeBody = true) const;
	/** Player faces within Angle degrees (sideways) of Location. */
	bool IsFacingTowards(const APawn* Player, const FVector& Location, float Angle) const;
	/**
	 * A stalker: the shortest walk to cover from Player (standing, or crouched behind something low if its body can
	 * crouch) that isn't closer to them and doesn't pass near them (false if none).
	 */
	bool FindHidingSpot(const APawn* Player, FVector& OutLocation, bool& bOutCrouch, bool& bOutCovered) const;
	/** A stalker: a place about StalkDistance behind Player that they can't see (false if none). */
	bool FindStalkSpot(const APawn* Player, FVector& OutLocation) const;
	/** A stalker: any player could see it at FeetLocation (a clear line to the middle of it, in range), facing it or not. */
	bool IsExposedAt(const FVector& FeetLocation, bool bCrouched) const;
	/** A stalker: some player would see it at FeetLocation with a glance (a clear line, within their view angle plus 45 degrees). */
	bool IsWatchedAt(const FVector& FeetLocation, bool bCrouched) const;
	/** A stalker: how much of a walk along Path (cm) some player could see it on (sampled every 1.5 m). */
	float ExposedLength(const UNavigationPath* Path, bool bCrouched) const;
	/** Path comes within Distance of Location. */
	static bool PassesNear(const UNavigationPath* Path, const FVector& Location, float Distance);
	/** Which way Player is heading: where they walk while walking steadily, else where they face (flat). */
	FVector GetQuarryForward(const APawn* Player) const;
	/** A stalker: Player has an opening for a strike (reloading, shooting, standing still a while, or a straggler). */
	bool HasOpening(const APawn* Player) const;
	/** How far Player is from the nearest other player who is up (0 when they are alone in the game). */
	double DistanceFromTeam(const APawn* Player) const;
	/** A stalker picking its prey: the player it would rather follow (the straggler or the most hurt), or Current. */
	APawn* ChoosePrey(APawn* Current) const;
	/** A stalker: a hidden place beside Player's way ahead of them, that it can reach first out of their sight. */
	bool FindWaylaySpot(const APawn* Player, FVector& OutLocation) const;
	/** A stalker showing itself: a place in Player's view far off (OutShow), with cover a step away (OutCover) that it can
	 *  reach unseen. */
	bool FindShowSpots(const APawn* Player, FVector& OutShow, FVector& OutCover) const;
	/** A stalker: starts following Player. */
	void Hook(APawn* Player);
	/** A stalker: goes to hide from Player (gasping, if bSpotted). */
	void Hide(const APawn* Player, bool bSpotted);
	/** Hunter and ambusher voices: alert as a chase starts, now and then while it goes on. */
	void UpdateChaseVoice();
	FVector GetFeetLocation() const;
	float GapTo(const AActor* Other) const;
	double Now() const;
	float RandomIn(const FVector2D& Range) const;
	void SetState(ETHEnemyState NewState) { State = NewState; }

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY()
	TObjectPtr<UTHEnemyBehavior> Behavior;

	TWeakObjectPtr<ATHEnemyCharacter> Enemy;
	TMap<TWeakObjectPtr<APawn>, FKnownPlayer> Known;
	TWeakObjectPtr<APawn> Target;
	ETHEnemyState State = ETHEnemyState::Idle;
	FVector Home = FVector::ZeroVector;

	// Moving
	FVector MoveGoal = FVector::ZeroVector;
	TWeakObjectPtr<AActor> MoveGoalActor;
	EPathFollowingRequestResult::Type LastMoveRequest = EPathFollowingRequestResult::Failed;
	double LastMoveRequestTime = -1.0e9;

	// A noise to look into
	bool bHasNoise = false;
	bool bUrgentNoise = false;
	FVector NoiseLocation = FVector::ZeroVector;
	double NoiseTime = -1.0e9;
	double LookAroundUntil = 0.0;
	/** Who made the last noise heard. */
	TWeakObjectPtr<APawn> NoiseMaker;

	double NextWanderTime = 0.0;
	/** Tactics: its flank point, its zig-zag side and when it flips, when it last tried doors. */
	FVector FlankPoint = FVector::ZeroVector;
	double NextFlankPick = 0.0;
	bool bFlanking = false;
	float WeaveSide = 1.0f;
	double NextWeaveFlip = 0.0;
	double NextDoorCheck = 0.0;

	/** Pressed against a closed door on its way after someone (Bangs On Doors): bangs on it. True while it does (it does nothing else). */
	bool BangOnDoorAhead();
	TWeakObjectPtr<UTHDoorSyncComponent> BangingOn;
	double BangUntil = 0.0;
	double NextBangCheck = 0.0;
	/** A charger: the last door it slammed (its sound once a door). */
	TWeakObjectPtr<UTHDoorSyncComponent> LastSlammedDoor;
	/** Its own (Variation, Reaction Time), picked as its behaviour is applied. */
	float SpeedVariation = 1.0f;
	float CooldownVariation = 1.0f;
	float ReactionDelay = 0.3f;
	/** Close to its target but blocked: where it is going round to for a clear line. */
	FVector ReachSpot = FVector::ZeroVector;
	bool bHasReachSpot = false;
	double NextReachPick = 0.0;
	double LastDamagedTime = -1.0e9;
	/** Shot: staggering until then; the next stagger no sooner than NextStaggerTime. */
	double StaggerUntil = 0.0;
	double NextStaggerTime = 0.0;
	double LastBusyTime = -1.0e9;

	// Stalker
	double NextStrikeTime = 0.0;
	double StrikeStartTime = 0.0;
	bool bStriking = false;
	bool bStrikeAttacked = false;
	/** A player came within PounceDistance: it goes straight for them (no sneaking, no moan). */
	bool bPouncing = false;
	/** A pounce from further off (cornered, or a standoff): kept up until its blow, seen or not. */
	bool bRushing = false;
	double LastRushTime = -1.0e9;
	/** Since when it has been held in a standoff with its prey (-1: not now). */
	double StandoffSince = -1.0;
	/** Strikes may come this soon at an opening (OpportunityDelay). */
	double NextOpportunityTime = 0.0;
	/** Since when it has been in someone's view (-1: not now); far off, it only ducks for SightingGrace. */
	double SeenSince = -1.0;
	/** Crossing ground a player could see: dashing (ExposedSpeed). Checked every 0.2 s. */
	bool bExposed = false;
	double NextExposureCheck = 0.0;
	/** The speed its last move asked for (the dash goes above it while exposed). */
	float MoveBaseSpeed = 0.0f;
	/** Its prey's way: heading (flat, eased), since when walking / standing still (-1: not). */
	FVector QuarryHeading = FVector::ZeroVector;
	double QuarryMovingSince = -1.0;
	double QuarryStillSince = -1.0;
	double NextPreyCheck = 0.0;
	/** Gone ahead to wait beside the prey's way, until they pass (or WaylayUntil). */
	bool bWaylaying = false;
	FVector WaylaySpot = FVector::ZeroVector;
	double WaylayUntil = 0.0;
	double NextWaylayTime = 0.0;
	/** Showing itself: 0 no, 1 going to the cover beside the place, 2 stepping out, 3 in view, 4 back into cover. */
	int32 ShowPhase = 0;
	FVector ShowSpot = FVector::ZeroVector;
	FVector ShowCover = FVector::ZeroVector;
	double ShowPhaseEnd = 0.0;
	double NextShowTime = 0.0;
	/** A lure: the next no sooner than this; waiting for them to come, until LureUntil. */
	double NextLureTime = 0.0;
	double LureUntil = 0.0;
	int32 LuresPlayed = 0;
	double NextStalkPick = 0.0;
	bool bHooked = false;
	TWeakObjectPtr<APawn> Quarry;
	double LurkUntil = 0.0;
	double HideRepickTime = 0.0;
	double NextIdleVoice = 0.0;
	double NextSpottedVoice = 0.0;
	bool bMoaned = false;
	/** The hiding place it is going to is low cover: it crouches there. */
	bool bHideCrouched = false;
	/** Partly in view (an edge or the top of its head showing): edging round its cover into full cover. */
	bool bShifting = false;
	double NextShiftTime = 0.0;
	/** Getting stuck: since when it has been barely moving while trying to; where it got stuck going, until when. */
	double SlowSince = -1.0;
	FVector AvoidSpot = FVector::ZeroVector;
	double AvoidUntil = 0.0;
	/** Pacing: which of eight points round the circle it is heading for, and which way round. */
	// Charger
	TWeakObjectPtr<APawn> ChargeTarget;
	double PhaseEnd = 0.0;
	double ChargeStartTime = 0.0;
	FVector ChargeDirection = FVector::ForwardVector;
	FVector ChargeStart = FVector::ZeroVector;
	/** Warnings played in this roar. */
	int32 TellsPlayed = 0;
	/** A roar called off (no clear run as it ended): the next no sooner than this. */
	double NextRoarTime = 0.0;

	int32 PaceStep = 0;
	int32 PaceDirection = 1;
	/** A player this close sets it off (RetreatDistance, varied each time it settles). */
	float ApproachDistance = 700.0f;
	/** Cornered (nowhere out of sight to hide) with a player this close: it attacks instead. */
	static constexpr float CorneredDistance = 600.0f;

	// Voices (hunter, ambusher)
	bool bWasChasing = false;
	double ChaseEndedTime = -1.0e9;
	double NextChaseVoice = 0.0;
	/** Called in by another's scream (or the director): the chase that starts doesn't scream or call others in turn. */
	bool bCalledIn = false;

	// Ambusher
	bool bAmbushSprung = false;
	/** Sprung at least once: it no longer lies down to play dead (it stands, head down). */
	bool bEverSprung = false;
	/** It waits lying down (picked once, on ApplyBehavior: the placed enemy's Wait Pose or tag, else the behaviour's chance). */
	bool bWaitsLying = false;
	/** A siren playing dead (shot down), and whether it has (once only). */
	bool bPlayingDead = false;
	bool bPlayedDead = false;
	double PlayDeadUntil = 0.0;
	/** Backing off from a flashlight: until then; the next no sooner than NextShunTime. */
	double ShunUntil = 0.0;
	double NextShunTime = 0.0;
};

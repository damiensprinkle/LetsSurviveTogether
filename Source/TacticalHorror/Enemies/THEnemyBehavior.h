#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "THEnemyBehavior.generated.h"

class USoundAttenuation;
class USoundBase;

/** What an enemy does about players (GDD section 7's archetypes). The logic is in ATHEnemyAIController. */
UENUM(BlueprintType)
enum class ETHEnemyBehaviorMode : uint8
{
	/** Wanders near where it was placed (or stands), walks to noises it hears, and runs at and attacks any player it sees or who shoots it. */
	Hunter,
	/**
	 * Lies hidden and unaware until it hears a player (or is shot, or a player comes right up to it); then it follows
	 * that player, moving only between places they can't see while they look away, and hides (the nearest place out of
	 * their view) when they turn towards it. Now and then it strikes from behind, moaning when right behind them, then
	 * hides again.
	 */
	Stalker,
	/**
	 * Waits motionless in a pack, head down, ignoring what's further than AmbushDistance (seen) or AmbushHearingDistance
	 * (heard); bursts out at a player who comes close, makes a sound near it or shoots it, and sets off the rest of its
	 * pack (PackAlertRadius); then hunts.
	 */
	Ambusher,
	/**
	 * A heavily mutated rarity (developer, 2026-11-12): spotting a player at a distance it roars (the warning), then
	 * charges in a nearly straight line, very fast. A hit hurts badly; a miss (sidestepped, a wall, too far) sends it
	 * sprawling on the ground for a long while, time to run or shoot. Close up it fights like a hunter.
	 */
	Charger,
	/**
	 * Barely turned, still looks like a person (developer, 2026-11-12): lies on its back (or stands) calling for help,
	 * ignoring being seen; a player who comes close (or shoots it) and it gets up and goes for them, then hunts like a
	 * hunter. A person's animations only.
	 */
	Siren
};

/**
 * One way an enemy behaves, with its tuning. Pick one on each placed enemy (Details panel: Enemy > Behavior), or none
 * for an enemy that just stands there. Make a new one by duplicating an asset in /Game/Enemies/Behaviors and changing
 * its numbers (Scripts/Tools/setup_enemies.py makes Hunter, Stalker and Ambusher).
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHEnemyBehavior : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	ETHEnemyBehaviorMode Mode = ETHEnemyBehaviorMode::Hunter;

	/** Full health with this behaviour; 0 = the body's own (200). Ambushers: 90, one headshot (an M4 does about 132, a G17 104). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = 0))
	float Health = 0.0f;

	// Senses (sight needs a line of sight; hearing goes through walls)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, Units = "cm"))
	float SightRadius = 2500.0f;

	/** Once seen, a player stays seen out to here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, Units = "cm"))
	float LoseSightRadius = 3000.0f;

	/** Half the field of view (90 = everything in front). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, ClampMax = 180, Units = "deg"))
	float SightHalfAngle = 70.0f;

	/**
	 * Hears a noise of loudness 1 (a sprinting footstep) within this distance; louder noises further, in proportion
	 * (UTHFootstepComponent's ...Noise values, UTHWeaponData::NoiseLoudness: a shot is 4). At 1000: a crouched step
	 * 1.2 m, a quiet walk 2.5 m, a walk 6 m, a landing 8 m, a shot 40 m (developer, 2026-11-11: 1500 felt too keen).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, Units = "cm"))
	float HearingRange = 1000.0f;

	/**
	 * Each wall, closed door or big prop between it and a noise cuts how far it carries to this much (developer,
	 * 2026-11-13: sound should be blocked by walls). At 0.35: a shot heard 40 m in the open is 14 m through one wall,
	 * 5 m through two; a walk (6 m) only 2 m through one. 1 = walls don't matter; 0 = nothing through a wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, ClampMax = 1))
	float WallMuffle = 0.35f;

	/**
	 * In the dark it makes a player out only this close (developer, 2026-11-13: light and dark in what enemies see); lit
	 * (their own light on, or a lamp on them) as far as it sees. 0 = light doesn't matter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, Units = "cm"))
	float DarkSightDistance = 0.0f;

	/**
	 * Each enemy its own (developer, 2026-11-13: a group never moves in step): its speeds and the time between its blows
	 * are each this much either way, picked at random as it starts. 0 = all alike.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, ClampMax = 0.5))
	float Variation = 0.12f;

	/** Seeing a player it wasn't after, it takes this long to react (random between the two, per enemy); shot, or with them
	 *  right up to it, at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (Units = "s"))
	FVector2D ReactionTime = FVector2D(0.15f, 0.6f);

	/** How long it keeps after a player it can no longer see or hear (it goes to where it last knew them to be). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = 0, Units = "s"))
	float MemoryTime = 8.0f;

	/**
	 * A flashlight beam on it (inside the beam's cone and range, nothing in between) gives the player holding it away,
	 * wherever it's looking: it goes for them (developer, 2026-11-11: hunters react to light).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	bool bReactsToLight = false;

	// Movement

	/** Wandering, investigating a noise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "cm/s"))
	float WalkSpeed = 80.0f;

	/** Chasing, slipping away. Players move at 260 cm/s unarmed, less with a gun (UTHCharacterMovementComponent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "cm/s"))
	float RunSpeed = 320.0f;

	/** Wanders to random places this far from where it was placed when there is nothing to do; 0 = stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "cm"))
	float WanderRadius = 600.0f;

	/** Seconds it stands between wanders (random between the two). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	FVector2D WanderPause = FVector2D(3.0f, 8.0f);

	/** At a noise it walked to, how long it looks around before giving up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "s"))
	float LookAroundTime = 4.0f;

	/**
	 * A noise it hasn't reached this long after hearing it (a closed door in the way that it bangs on, or no way there) is
	 * given up (2026-11-14: it banged on the door for good). Long enough to run to a shot heard 40 m off. 0 = never gives up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "s"))
	float InvestigateGiveUpTime = 20.0f;

	/**
	 * With nothing to do it paces a circle this wide around where it was placed, walking like a person (the body set's
	 * Calm Walk), instead of wandering and standing (developer, 2026-11-11: hunters pace, so players learn them). 0 = it
	 * wanders (Wander Radius) or stands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "cm"))
	float PaceRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0, Units = "cm/s"))
	float PaceSpeed = 140.0f;

	// Hunting tactics (developer, 2026-11-13: hunters smarter: flanking, searching ahead, doors, weaving, a scream)

	/** Two or more after the same player: the nearest comes straight on, the others circle to their side or back first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics")
	bool bFlanks = false;

	/** How far to the player's side (or behind them) a flanker goes before it closes in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (EditCondition = "bFlanks", Units = "cm"))
	float FlankDistance = 450.0f;

	/** Lost sight of a player: it heads for where they were going (their last pace, up to this far on), not where it saw them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (ClampMin = 0, Units = "cm"))
	float SearchAhead = 600.0f;

	/** A player aiming straight at it (within Weave Angle) between 4 and 15 m: it zig-zags this far either side as it comes. 0 = straight on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (ClampMin = 0, Units = "cm"))
	float WeaveOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (ClampMin = 0, Units = "deg"))
	float WeaveAngle = 12.0f;

	/** Starting a chase, its shout brings others of its kind within this far, running to that player. 0 = no one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (ClampMin = 0, Units = "cm"))
	float ScreamRadius = 0.0f;

	/** Its shout as a chase starts and it calls others in (Scream Radius above 0), instead of its Alert Sounds: one sound
	 *  players learn means more are coming (developer, 2026-11-14). Empty = Alert Sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics")
	TArray<TObjectPtr<USoundBase>> ScreamSounds;

	/** Opens closed (unlocked) doors in its way (only the one it walks into; the door's own sound plays). Only the stalker
	 *  (developer, 2026-11-13); the others stop at a closed door. A charger slams doors open whatever this says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics")
	bool bOpensDoors = false;

	/** Stopped by a closed door while after a player (or a noise): it bangs on it, a swing of its arms and a bang at the door,
	 *  until the door opens or it gives up the chase (developer, 2026-11-13: hunters and ambushers slamming on doors). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics")
	bool bBangsOnDoors = false;

	/** Banging on a door: one of these, at random, at the door with each blow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (EditCondition = "bBangsOnDoors"))
	TArray<TObjectPtr<USoundBase>> DoorBangSounds;

	/** Seconds from one blow on a door to the next (at random in this range; at least its swing's length). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (EditCondition = "bBangsOnDoors", Units = "s"))
	FVector2D DoorBangInterval = FVector2D(0.9, 1.6);

	/** Each blow on a door shakes the view of players this close (more the closer; through the door too). 0 = no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics", meta = (EditCondition = "bBangsOnDoors", Units = "cm"))
	float DoorBangShakeRadius = 600.0f;

	/** A flashlight on it from more than 3 m: it backs off 4 m into the dark for a moment (at most every 4 s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactics")
	bool bShunsLight = false;

	// Attack

	/** Reach, from its capsule's edge to the player's capsule's edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0, Units = "cm"))
	float AttackRange = 80.0f;

	/** Per blow (players have 100). The difficulty setting will scale this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0))
	float AttackDamage = 35.0f;

	/** Seconds between the end of one attack and the start of the next. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0, Units = "s"))
	float AttackCooldown = 0.6f;

	/** Keeps attacking a downed player until they die (the developer's rule, 2026-11-09); off = leaves them be. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bFinishOffDowned = true;

	/** Shot: it staggers (the body set's Hit Reactions, one at random) and stops for this long (developer, 2026-11-13: the
	 *  hunter, stalker and siren should stagger slightly when shot). 0 = it shrugs shots off (a charger). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (Units = "s", ClampMin = 0))
	float StaggerTime = 0.0f;

	/** A headshot or a big hit (a quarter of its health or more): the stagger lasts this much longer, a heavy flinch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 1))
	float HeavyStaggerScale = 1.8f;

	/**
	 * Wounds (developer, 2026-11-13: hits that matter; halved the same day: limbs only came off corpses). Damage to one limb (its bones) adds up: this much and it comes
	 * off (if its look has the piece; the head only on a killing headshot of Head Off Damage). 0 = limbs never come off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wounds", meta = (ClampMin = 0))
	float LimbHealth = 45.0f;

	/** A killing headshot this strong takes the head off. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wounds", meta = (ClampMin = 0))
	float HeadOffDamage = 90.0f;

	/** A leg hurt by half its Limb Health (or more): it limps, all its speeds times this. 0.8: a limping hunter (330 cm/s) still
	 *  runs 264, faster than a player with a rifle (225); at 0.65 one leg hit meant it could never catch anyone (2026-11-13). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wounds", meta = (ClampMin = 0, ClampMax = 1))
	float LimpSpeedScale = 0.8f;

	/** Each arm lost: its blows do this much of their damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wounds", meta = (ClampMin = 0, ClampMax = 1))
	float LostArmDamageScale = 0.6f;

	/** At most one stagger this often, so automatic fire can't keep it pinned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (Units = "s", ClampMin = 0))
	float StaggerCooldown = 1.0f;

	// Stalker

	/** How far behind a player it likes to keep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float StalkDistance = 1200.0f;

	/** Moving while stalking (quietly). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm/s"))
	float StalkSpeed = 180.0f;

	/** Slipping away (looked at, too close, shot, or after a strike): fast, so it is gone before it is clearly seen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm/s"))
	float RetreatSpeed = 330.0f;

	/** A player coming about this close (20% either way, picked each time it settles) makes it go and hide further away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float RetreatDistance = 700.0f;

	/** Where it goes to hide is at least this far from the player (and further than it already was): a real move, not a hop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float HideDistance = 600.0f;

	/**
	 * It counts as in a player's view within this many degrees either side of where they face (sideways only: looking
	 * at the floor doesn't hide it) with a clear line from their eyes. In view, it hides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "deg"))
	float ViewAngle = 60.0f;

	/** Lurks and creeps crouched (the body set's Crouch and CrouchWalk), and stays crouched to run and strike too: getting up and
	 * down looked wrong (developer, 2026-11-11). Off: it stands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	bool bCrouches = true;

	/** Hidden, it waits this long (random between the two), and while the player can see where it is, before moving again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	FVector2D LurkTime = FVector2D(2.0f, 5.0f);

	/** Unaware, a player this close notices it even without a sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float WakeDistance = 300.0f;

	/**
	 * Always looking for players: it senses any player this close, through walls, and starts stalking them (developer,
	 * 2026-11-12: it should hunt for the player, not wait to be found). 0 = only sounds, being looked at, or Wake Distance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float SenseDistance = 2500.0f;

	/** Strikes from behind now and then; off = only ever follows (pure psychological pressure). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	bool bStrikes = true;

	/** Seconds of stalking before a strike (random between the two), counted again after each. The longest it waits: an
	 *  opening (Opportunity Delay) makes it strike sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bStrikes", EditConditionHides))
	FVector2D StrikeDelay = FVector2D(15.0f, 30.0f);

	/** A strike that hasn't landed a blow by then is given up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bStrikes", EditConditionHides, Units = "s"))
	float StrikeTimeout = 8.0f;

	/**
	 * A player this close (seen or not, coming at it or not; or still this close after its strike) and it just goes for
	 * them instead of hiding, and keeps at them until they are 1.5x this away (developer, 2026-11-13: it was too focused
	 * on fleeing; it attacked once and ran off). 0 = never.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bStrikes", EditConditionHides, Units = "cm"))
	float PounceDistance = 500.0f;

	/**
	 * Trapped in a standoff (developer, 2026-11-15: it sat crouched in a room while the player blocked the doorway): within
	 * Standoff Distance of its prey, faced by them, and either in their view or not moving (not in one of its own waits), for
	 * this long, and it goes for them, seen or not, until a blow or until they are 1.5x Standoff Distance away. 0 = never.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bStrikes", EditConditionHides, Units = "s"))
	float StandoffTime = 6.0f;

	/** How close a standoff counts (a room's length: the doorway may be the far side of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bStrikes", EditConditionHides, Units = "cm"))
	float StandoffDistance = 1200.0f;

	/** Hides behind low cover (crates), crouched. Off: only things that hide it standing (walls, doorways, tall props)
	 *  count when it picks where to hide or watch from (developer, 2026-11-13: hiding behind crates just didn't work). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	bool bUsesLowCover = false;

	// Stalker as a predator (developer, 2026-11-13: it should treat players as prey, moving round behind walls, running
	// to avoid being seen; it reacted to the player more than it hunted)

	/** Crossing ground a player could see it on (a clear line, even if they face away): it dashes across at this speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm/s"))
	float ExposedSpeed = 450.0f;

	/** Choosing where to go, each metre of the walk a player could see it on costs this many metres of extra walking:
	 *  it takes the long way round, behind walls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	float ExposureCost = 4.0f;

	/** Seen further away than this: it ducks behind the nearest cover (a few steps, fast) instead of running off to hide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float FarSightingDistance = 1500.0f;

	/** Seen far away for longer than this (or with no cover near): it hides properly after all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "s"))
	float SightingGrace = 1.5f;

	/**
	 * An opening (the player reloading, shooting at something, standing still for 2 s, or far from their teammates) and it
	 * strikes from behind before Strike Delay is up, at most this often (random between the two). 0 = only Strike Delay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bStrikes", EditConditionHides))
	FVector2D OpportunityDelay = FVector2D(5.0f, 10.0f);

	/** A player this far from every other player is a straggler: an opening, and the prey it prefers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float StragglerDistance = 1500.0f;

	/** Picks its prey among the players it senses: the straggler or the most hurt; off = keeps to the first it found. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	bool bPicksPrey = true;

	/** A player walking steadily one way: the chance, each time it picks somewhere (about every 10 s at most), that it
	 *  races ahead of them out of sight and waits beside their way (to strike as they pass). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, ClampMin = "0", ClampMax = "1"))
	float WaylayChance = 0.35f;

	/** How far ahead of a walking player it lies in wait. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float WaylayAhead = 900.0f;

	/** Within this of a player, their lights flicker (developer, 2026-11-13: it plays with your flashlight when close; worse the
	 *  closer). 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides, Units = "cm"))
	float LightFlickerDistance = 0.0f;

	/** Copies sounds to lure a player to it (one at random: a teammate's cry, steps), from where it waits out of sight 8 to 20 m
	 *  off, then waits there for them (developer, 2026-11-13). Empty = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	TArray<TObjectPtr<USoundBase>> LureSounds;

	/** Seconds between lures (random between the two). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	FVector2D LureInterval = FVector2D(25.0f, 50.0f);

	/** Now and then it lets itself be seen on purpose: steps out into a player's view far off, stays a moment, and is gone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker", EditConditionHides))
	bool bShowsItself = true;

	/** Seconds between showings (random between the two; the first counts from when it starts following someone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bShowsItself", EditConditionHides))
	FVector2D ShowInterval = FVector2D(45.0f, 90.0f);

	/** How far from the player it shows itself (between the two). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bShowsItself", EditConditionHides))
	FVector2D ShowDistance = FVector2D(1000.0f, 2000.0f);

	/** How long it stays in view when it shows itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Stalker && bShowsItself", EditConditionHides, Units = "s"))
	float ShowTime = 1.2f;


	// Ambusher

	/** While waiting it reacts to a player seen within this distance (or right next to it, 40% of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambusher", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Ambusher", EditConditionHides, Units = "cm"))
	float AmbushDistance = 300.0f;

	/** While waiting it reacts to a player's sound (a step, a shot) made within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambusher", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Ambusher", EditConditionHides, Units = "cm"))
	float AmbushHearingDistance = 500.0f;

	/** Springing, it sets off every waiting ambusher within this distance at the same player (a pack); 0 = alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambusher", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Ambusher", EditConditionHides, Units = "cm"))
	float PackAlertRadius = 1200.0f;

	/** Waiting, it stares at the floor: head and neck bent down this far (0 = looks ahead). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambusher", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Ambusher", EditConditionHides, ClampMin = 0, ClampMax = 80, Units = "deg"))
	float WaitingHeadDown = 50.0f;

	// Charger

	/** It charges at a player it sees at least this far away; closer, it fights like a hunter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "cm"))
	float ChargeMinDistance = 500.0f;

	/** How long it stands roaring before it charges: the players' warning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "s"))
	float RoarTime = 1.6f;

	/** As the roar ends it looks again: no clear run (they stepped behind something), no charge; it comes round instead and
	 *  roars again no sooner than this (developer, 2026-11-15: it charged into objects). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "s"))
	float RoarRetryDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "cm/s"))
	float ChargeSpeed = 620.0f;

	/** Degrees a second it can turn while charging: low, so a sidestep makes it miss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "deg"))
	float ChargeTurnRate = 35.0f;

	/** A charge ends (a miss) after this far. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "cm"))
	float ChargeMaxDistance = 2500.0f;

	/** The charge's blow (its other attacks use Attack Damage): 100 kills a player at full health (developer, 2026-11-12). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides))
	float ChargeDamage = 100.0f;

	/** Its warning as it roars (developer, 2026-11-13: a floor scrape before the charge): played this many times, spread over the
	 *  roar, one of these at random each time (stomps for now; no scrape sound in the packs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides))
	TArray<TObjectPtr<USoundBase>> ChargeTellSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, ClampMin = 0))
	int32 ChargeTellCount = 3;

	/** After a miss: seconds on the ground between going down and getting up (the whole stumble is longer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "s"))
	float StumbleDownTime = 4.0f;

	/** After a hit: seconds it stands, recovering, before it fights on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "s"))
	float ChargeRecoverTime = 1.5f;

	/** Running into a wall: particles where it hits, facing out of the wall (empty: none yet; needs a dust or debris pack). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides))
	TObjectPtr<class UNiagaraSystem> WallCrashEffect;

	/** A looping effect (Vefects' wind dust: developer, 2026-11-16) spawns only this long, then its particles live out their
	 *  lives: a puff. 0 = it plays as made (a one-shot ends itself). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "s", ClampMin = 0))
	float WallCrashEffectTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, ClampMin = 0.01))
	float WallCrashEffectScale = 1.0f;

	/** Running into a wall: one of these, at random, played there (empty: none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides))
	TArray<TObjectPtr<USoundBase>> WallCrashSounds;

	/** Slamming a door open as it charges through: one of these at the door, with the wall crash's shake (empty: only the
	 *  door's own sound). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides))
	TArray<TObjectPtr<USoundBase>> DoorSlamSounds;

	/** A wall (or crate) this close in front of a charge, square on, is a crash; it doesn't slide along it (developer, 2026-11-12). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "cm"))
	float WallCrashReach = 40.0f;

	/** Running into a wall shakes the view of players within this distance, more the closer they are. 0 = no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, Units = "cm"))
	float WallCrashShakeRadius = 1500.0f;

	/** The shake right next to it (1 = a heavy blow's jolt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charger", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Charger", EditConditionHides, ClampMin = 0))
	float WallCrashShake = 1.2f;

	/** This behaviour's own looks (a charger: the mutants), used instead of the enemy's Appearance; none = the enemy's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	TObjectPtr<class UTHEnemyAppearance> Appearance;

	/** This behaviour's own animations (a siren: a person's), used instead of the enemy's; none = the enemy's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	TObjectPtr<class UTHEnemyAnimSet> Animations;

	// Siren

	/** It waits lying down instead of standing: a siren on its back or stomach calling for help; an ambusher playing dead among
	 *  the bodies until it springs (developer, 2026-11-13). The animations' Lying Poses, at random, else Stumble Down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Siren || Mode == ETHEnemyBehaviorMode::Ambusher", EditConditionHides))
	bool bLiesDown = true;

	/** Lies Down on: the chance each one does (picked once, as it starts); the rest wait standing (an ambusher head down). A
	 *  placed enemy can choose for itself (its Wait Pose, or an actor tag Lying / Standing; developer, 2026-11-14). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "bLiesDown && (Mode == ETHEnemyBehaviorMode::Siren || Mode == ETHEnemyBehaviorMode::Ambusher)", EditConditionHides, ClampMin = 0, ClampMax = 1))
	float LiesDownChance = 1.0f;

	/** A player this close (and in sight) and it turns on them; seen from further, it keeps calling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Siren", EditConditionHides, Units = "cm"))
	float SirenDistance = 250.0f;

	/**
	 * Lying, a player this close in sight: it beckons them (its lying pose's Reacting clip, a weak wave) to seem more
	 * inviting (developer, 2026-11-13). 0 = never.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Siren", EditConditionHides, Units = "cm"))
	float BeckonDistance = 1000.0f;

	/**
	 * It works on one player at a time (developer, 2026-11-13: call one player away from the group): with more than one in
	 * sight it lies still and quiet; it calls, beckons and crawls only for someone alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Siren", EditConditionHides))
	bool bOnlyForOne = true;

	/** A lone player in sight within Beckon Distance: it crawls towards them, "hurt", with these animations (none = it stays put). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Siren", EditConditionHides))
	TObjectPtr<class UTHEnemyAnimSet> CrawlAnimations;

	/** Turned on them and shot down to this much of its health: it drops and plays dead, once, until someone comes close or shoots it. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Siren", meta = (EditCondition = "Mode == ETHEnemyBehaviorMode::Siren", EditConditionHides, ClampMin = 0, ClampMax = 1))
	float PlayDeadHealth = 0.5f;

	// Voice (played from the enemy on every machine; one at a time; empty = silent)

	/** Noticing a player: a hunter starting a chase, an ambusher springing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TArray<TObjectPtr<USoundBase>> AlertSounds;

	/** Now and then while chasing (every ChaseSoundInterval seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TArray<TObjectPtr<USoundBase>> ChaseSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FVector2D ChaseSoundInterval = FVector2D(5.0f, 9.0f);

	/** A stalker seen: as it goes to hide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TArray<TObjectPtr<USoundBase>> SpottedSounds;

	/**
	 * Now and then, every IdleSoundInterval seconds: a stalker's breathing while it follows a player; a hunter's calls
	 * while it paces with nothing to chase (calling players to it).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TArray<TObjectPtr<USoundBase>> IdleSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FVector2D IdleSoundInterval = FVector2D(15.0f, 35.0f);

	/** Right behind a player, about to strike (CloseSoundDistance): the stalker's warning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TArray<TObjectPtr<USoundBase>> CloseSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice", meta = (Units = "cm"))
	float CloseSoundDistance = 350.0f;

	/** How far the voice carries (none = the sound's own). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundAttenuation> VoiceAttenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice", meta = (ClampMin = 0))
	float VoiceVolume = 1.0f;
};

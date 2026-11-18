# Enemies — quick reference

How each enemy type behaves and every value you can tune. Values are what's in the assets and the setup scripts now (2026-11-14). The
code behind it is described in TECHNICAL_ARCHITECTURE.md (*Enemies*). Things still to try in play: ENEMY_PLAYTESTS.md.

## In the story (developer, 2026-11-13; GDD.md section 9)

Every enemy is a person infected by **the Quorum**, the organism found on the Devil's Tooth fungus at the Aegis
facility. They work together, but some behave differently:

- **Siren:** looks the most human, speaks English, indistinguishable from anyone else. The first one players meet is
  the guard in the lab's utility closet (a cutscene; GDD 9.5).
- **Ambusher:** stands together in small groups and doesn't react unless you are close.
- **Hunter:** waits for its moment and charges you down. The first fight is Hunters at the security checkpoint
  (1 for 1-2 players, 2 for 3, 3 for 4).
- **Stalker:** seems to enjoy creeping around; walks behind you and makes little moans before attacking. It doesn't
  speak or whisper (developer, 2026-11-14); a sinister laugh may be added if a sound is found (ENEMY_ASSETS_TODO.md).
- **Charger:** mutated, no longer looks human; brute force, strong enough to break down doors and containment.

## Setting an enemy up

- Place `/Game/Enemies/BP_Zombie` and pick its **Behavior** in the Details panel (**Enemy > Behavior**):
  `DA_Hunter`, `DA_Stalker`, `DA_Ambusher`, `DA_Charger` or `DA_Siren` (in `/Game/Enemies/Behaviors`). Empty = it just stands there.
- **Looks are random** (2026-11-11): `BP_Zombie` has **Appearance** = `DA_ZombieLooks` (`/Game/Enemies/Appearance`), so each
  zombie picks a model, skin and gore, clothing and hair at random as the level starts, the same for every player. To
  pick one body yourself, clear **Appearance** on the placed enemy and set **Body Mesh** (the line-up does this).
- To make a variant (a faster hunter, a quieter stalker), duplicate a behaviour asset, change its numbers, and pick
  the copy. Re-running `setup_enemies.py` never overwrites behaviour values you have changed.
- In the editor, each behaviour only shows the settings for its type (the others are hidden).
- Console: `th.EnemyDebug 1` prints on screen what each enemy sees and hears.
- Each enemy speaks with its own voice pitch (0.88 to 1.12), so a group doesn't sound like one person.

## Hunter (`DA_Hunter`)

With nothing to do it **paces a circle** about its spot, **walking like a person** (calm walk), and now and then
**calls out** (hurt and scared sounds, calling players to it; the voice pack has no words, so real calls for help need a voice pack).
Walks to footsteps it hears and runs to gunshots. Runs at any player it sees, who shoots it, or who **shines a
flashlight on it** (weapon light or hand flashlight, wherever it's looking), and attacks. If it loses you, it goes to
where it last saw you and looks around. Shouts (angry) when a chase starts and every few seconds during it. It only swings with **nothing in between** (developer, 2026-11-13: it should wait for a clear line): with a crate or door frame between you, it goes round to a spot with a clear line, or waits for one.
**Tactics** (2026-11-13, developer: smarter hunters; `setup_tactics.py`): with two or more after you, the nearest comes straight on and the others **circle** to your side or behind you (4.5 m out) before closing in. Starting a chase, its **scream** (its own sound, so players learn it: `HunterScream`, 2026-11-14) brings other hunters within 18 m running to you; a hunter that was called comes with its ordinary shout and doesn't call more (2026-11-14: one scream ran on from hunter to hunter). Aimed at (within 12° of where you aim, 4 to 15 m, a clear line), it **zig-zags** 1.6 m either side as it comes. Losing sight of you, it **searches ahead**: where you were heading (two seconds at your pace, 6 m at most), not just where it saw you. It does **not** open doors (developer, 2026-11-13: only the stalker does): stopped by a closed door while after you (or a noise), it **bangs on it** (Bangs On Doors: one of its attack swings, a bang at the door with each blow (`DoorBang`), a slight shake for players within 6 m, every 0.9-1.6 s) until the door opens or it gives up (a chase after Memory Time; a noise it can't get to after Investigate Give Up Time, 20 s: it used to bang for good, 2026-11-14); ambushers do too.

| Setting | Value | What it does |
|---|---|---|
| Health | 0 (= body's 200) | Full health. M4: about 7 body shots or 2 headshots |
| Sight Radius | 2500 cm | Sees players this far when they are lit; in the dark only within Dark Sight Distance (1000 cm) |
| Lose Sight Radius | 3000 cm | Keeps seeing a player out to here |
| Sight Half Angle | 70° | Half its field of view |
| Hearing Range | 1000 cm | Hears a loudness-1 noise this far (see *Noises* below) |
| Memory Time | 8 s | Keeps after a player it can't sense for this long |
| Walk Speed | 80 cm/s | Wandering, investigating |
| Run Speed | 330 cm/s | Chasing (you: 260 unarmed, about 225 with a rifle) |
| Wander Radius | 600 cm | Wanders this far from its spot; 0 = stands still |
| Pace Radius | 250 cm | Paces a circle this wide about its spot (calm walk); 0 = wanders (Wander Radius) or stands |
| Pace Speed | 140 cm/s | Pacing |
| Reacts To Light | on | A flashlight beam on it gives the player away |
| Wander Pause | 3–8 s | Stands this long between wanders |
| Look Around Time | 4 s | At a noise or where it lost you, looks around this long |
| Investigate Give Up Time | 20 s | A noise it hasn't reached by then (a closed door in the way) is given up; 0 = never |
| Attack Range | 80 cm | Reach, capsule edge to capsule edge |
| Attack Damage | 55 | Per blow (players have 100, healing back after 5 s unhurt): two blows down you |
| Attack Cooldown | 0.6 s | Between blows |
| Finish Off Downed | on | Keeps hitting a downed player until they die |
| Alert / Chase Sounds | Angry ×4 | As a chase starts / every Chase Sound Interval |
| Chase Sound Interval | 5–9 s | |
| Scream Radius / Sounds | 1800 cm / `HunterScream` | Starting a chase it found itself: its scream, and hunters this close come running (they don't scream or call in turn) |
| Flanks / Flank Distance | on / 450 cm | With others after you, it circles to your side or back first |
| Weave Offset / Angle | 160 cm / 12° | Aimed at, it zig-zags this far either side |
| Search Ahead | 600 cm | Lost you: it looks this far along the way you were going |
| Dark Sight Distance | 1000 cm | In the dark it makes you out only this close |
| Opens Doors / Bangs On Doors | off / on (`DoorBang`) | Stopped by a closed door, it bangs on it |
| Idle Sounds / Interval | Injured ×4, Scared ×4 / 15–35 s | Its calls while pacing |

## Stalker (`DA_Stalker`)

Always **looking for players**: it senses any player within Sense Distance (25 m), **through walls**, and starts stalking them. Further off, it lies crouched until it **hears** a player (a step, a shot), is shot, or a player **looks at it**.
Then it follows that player, always knowing where they are, but keeps out of their sight. It is the **only enemy that opens doors** (developer, 2026-11-13): just the closed, unlocked door it walks into (a body-wide sweep along its way, not the doors it passes), and the door's own sound plays, so a door opening nearby may be it. It stays **crouched** the
whole time (lurking, creeping, running, striking):

- **Seen** (the middle of its chest or head in their sideways view, with a clear line of sight) or approached: it gasps and **hides**, running
  all the way to cover at least Hide Distance from them (it doesn't stop if you step back). It picks the best
  escape: far from you by walking distance (another room beats a corner of this one), never a corner or dead end.
  Cornered (nowhere out of sight to go) with you within 6 m, it attacks instead, and doesn't turn back when seen (it used
  to: a cornered strike counted as sneaking, and being seen called it off). **Standoff** (2026-11-15): within Standoff Distance
  (12 m) of you, you facing it, and it in your view or getting nowhere, for Standoff Time (6 s; not counting its own waits), it
  comes for you, seen or not, until its blow or until you are 1.5x that away (it sat crouched in a room while
  the player blocked the doorway).
- **Glimpsed** (only an edge showing: a shoulder past a crate, the top of its head over it): it creeps a few steps round
  its cover until none of it can be seen (2.5 m at most); with nowhere that near, it hides as if seen. Stuck against
  something on the way anywhere (a second barely moving), it gives that place up for 10 s and picks another. Hiding and
  stalking places must hide all of it (head top and shoulders too), not just the middle of its chest and head.
- **Too close** only counts when you walk towards it (or come within about 4 m); you just standing near it doesn't scare it.
- **Hidden:** waits Lurk Time, then moves again unless you're facing it with a clear line to it (facing it through a wall doesn't hold it; it slips round you); if it has fallen well behind (more than
  1.6 × Stalk Distance) it comes back regardless.
- **Stalking:** moves between places out of your view (behind your back, or behind a wall), about Stalk Distance from you, working its way **behind you**: it only settles and watches once it is behind you (in front, even out of sight, it keeps moving round). It settles only where it can see you, behind your back in the same room (2026-11-13: it settled in the next room and never came back); routes and its dash count only ground you would see with a glance (your view angle plus 45°), so it creeps up behind you.
- **Strike:** every Strike Delay, once it's outside your view (more than 75° off where you face), it comes in, **moans** right behind you, hits once, then hides.
- **Pounce:** within Pounce Distance (5 m) of you it stops hiding and **just attacks**, seen or not, and **keeps at you** until you get 7.5 m away (developer, 2026-11-13: it attacked once and ran off). After a strike from behind it keeps at you too if you are still within 5 m. Sneaking in for a strike it still moans; spotted on the way within 5 m, it attacks instead of hiding.
- **A predator, not just a follower** (developer, 2026-11-13): it picks its routes by how much of the way you could see it on, so it goes round **behind walls** and through the next room rather than across your view; crossing ground you could see (even facing away) it **dashes** (Exposed Speed), and creeps again behind cover. Seen **far off** (beyond 15 m) for a moment, it just ducks behind the nearest cover. With you walking steadily one way it may **race ahead** out of sight and wait beside your way (then it is behind you as you pass, or pounces if you come within 5 m). It strikes at **openings** (reloading, shooting at something, standing still, a straggler) instead of only on its timer. In co-op it **picks its prey**: the straggler or the most hurt. Now and then it **shows itself** far off on purpose for a moment, then is gone.
- **Plays with you** (2026-11-13, developer): within 7 m your **lights flicker** (all you carry; on each machine, for its own player): steady 0.6-1.6 s, then a dip or a cut-out for 0.12-0.35 s, more often the closer it is, each with an electrical **crackle** only you hear (`LightCrackle`; developer, 2026-11-14: it flickered too quickly). Every 25 to 50 s, out of your sight 8 to 20 m off, it makes a **footstep** (it doesn't speak; developer, 2026-11-14: no more copying your hurt cries) and **waits there** for you to come looking; coming at it then doesn't scare it off, and within 5 m it goes for you. Dragging a downed player off needs drag animations (none in the packs).
- Breathes (gasping) now and then while it follows you.
- **A downed player is not its prey** (2026-11-14): when the one it follows goes down it turns to a player still up (it used to freeze beside the downed one); with nobody up in reach it lies in wait again.

| Setting | Value | What it does |
|---|---|---|
| Health | 0 (= body's 200) | |
| Hearing Range | 1000 cm | What wakes it (with Wake Distance) |
| Sight Radius / Lose Sight / Half Angle | 2500 / 3000 cm / 80° | |
| Memory Time | 8 s | |
| Run Speed | 300 cm/s | Striking |
| Stalk Distance | 750 cm (was 1200) | How far behind you it likes to be: in the same room (2026-11-13: at 12 m it watched from the next room, through a wall) |
| Stalk Speed | 180 cm/s | Creeping (crouched) between hiding places |
| Retreat Speed | 330 cm/s | Running to hide |
| Retreat Distance | 550 cm (was 700) | You walking towards it inside this (±20%, varied each time), or within 55% of it, sends it to hide; it stalks from at least 1.3× this |
| Hide Distance | 600 cm (was 900) | Hiding places are at least this far from you |
| View Angle | 60° | Counts as in your view within this many degrees either side of where you face |
| Lurk Time | 2–5 s (was 4–8) | Waits hidden at least this long |
| Wake Distance | 300 cm | Unaware, notices a silent player this close |
| Sense Distance | 2500 cm | Always looking: senses any player this close, through walls, and starts stalking (0 = off) |
| Crouches | on | Stays crouched throughout (off: stands) |
| Strikes | on | Off = only ever follows (pure dread) |
| Strike Delay | 15–30 s | Before the first strike and between strikes |
| Pounce Distance | 500 cm | A player this close and it just **attacks** instead of hiding (seen or not); keeps at them until 1.5× this away (0 = never) |
| Standoff Time | 6 s | Held close by its prey (faced, and seen or getting nowhere) this long: it comes for them (0 = never) |
| Standoff Distance | 1200 cm | How close a standoff counts: a room's length (Stalk Distance, 7.5 m, was too short: a doorway can be further) |
| Uses Low Cover | off | Off: only walls, doorways and tall props count as hiding places, not crates (developer, 2026-11-13: hiding behind crates just didn't work) |
| Exposed Speed | 450 cm/s | Crossing ground a player could see it on (a clear line, even facing away): it dashes |
| Exposure Cost | 4 | Choosing a route, each metre a player could see it on costs this many metres of walking: it goes the long way round, behind walls |
| Far Sighting Distance | 1500 cm | Seen further off than this, it ducks behind the nearest cover instead of running off |
| Sighting Grace | 1.5 s | Seen far off longer than this (or no cover near), it hides properly |
| Opportunity Delay | 5–10 s | At an opening (you reloading, shooting, standing still 2 s, or a straggler) it strikes this soon after its last strike, before Strike Delay |
| Straggler Distance | 1500 cm | A player this far from every other player is a straggler: an opening, and the prey it prefers |
| Picks Prey | on | Follows the straggler or the most hurt player it senses; off = the first it found |
| Waylay Chance / Ahead | 0.35 / 900 cm | You walking steadily one way: the chance (each pick, at most every 10 s) it races ahead out of sight and waits this far along your way |
| Shows Itself | on | Now and then steps into your view far off on purpose, stays a moment, and is gone |
| Show Interval / Distance / Time | 45–90 s / 10–20 m / 1.2 s | How often, how far off, how long in view |
| Strike Timeout | 8 s | Gives a strike up after this long |
| Attack Range / Damage / Cooldown | 80 cm / 60 / 0.6 s | |
| Spotted Sounds | Gasping ×3 | When you see it |
| Idle Sounds / Interval | Gasping ×3 / 15–35 s | Breathing while it follows |
| Close Sounds / Distance | Moaning ×3 / 350 cm | Right behind you, before a strike |

## Ambusher (`DA_Ambusher`)

Waits in a **pack**, some **playing dead** on the floor among the bodies (developer, 2026-11-13: a pack of "dead" ambushers all jump up), the rest **standing**, head down (2026-11-14: 60% lie, Lies Down Chance; a placed one chooses with its **Wait Pose** in the Details panel, As Behaviour / Lying / Standing, or an actor tag `Lying` or `Standing`). Lying, it lies face down, springing up with Mixamo's zombie Stand Up (about 0.9 s; developer, 2026-11-14) (`setup_play_dead.py`; after its first hunt it waits standing, head down). Bursts out at a player it sees very close or hears
nearby (a step, a shot), with a **shriek** (`AmbusherShriek`; only the one that springs first: the pack it sets off springs silent, the clip is long; 2026-11-14), and sets off every waiting ambusher around it at the same player. Then it hunts like a hunter;
with nothing to go on for Memory Time, it waits again where it is. Dies to one headshot. Doesn't patrol. Sprung, it bursts up off the floor (about 0.9 s), and the pack **surrounds** you (the nearest comes on, the others circle 3 m out) rather than coming single file. A **flashlight** on it from more than 3 m and it backs off 4 m into the dark for a moment (at most every 4 s).

| Setting | Value | What it does |
|---|---|---|
| Health | 90 | One headshot (M4 about 132, G17 about 104); 3–4 body shots |
| Ambush Distance | 300 cm | Notices a player it sees this close (or right next to it, 40%) |
| Ambush Hearing Distance | 500 cm | Notices a player's sound made this close |
| Pack Alert Radius | 1200 cm | Springing, shot, or killed outright by a player, it sets off every waiting ambusher this close at that player (2026-11-13: a pack could be picked off one by one from beyond hearing); 0 = alone. Once shot it never lies down again |
| Waiting Head Down | 50° | How far it bows its head while waiting; 0 = looks ahead |
| Hearing Range | 1000 cm | The engine's hearing (Ambush Hearing Distance limits it further) |
| Sight Radius / Half Angle | 2500 cm / 90° | Once sprung |
| Memory Time | 8 s | |
| Run Speed | 380 cm/s | Faster than the hunter |
| Wander Radius | 0 | Stays put |
| Attack Range / Damage / Cooldown | 80 cm / 45 / 0.6 s | Lower than the rest: they come in packs |
| Alert / Chase Sounds | `AmbusherShriek` / Scared ×4 | The shriek only from the one that springs first |
| Lies Down / Lies Down Chance | on / 0.6 | How many of a pack play dead; the rest stand head down |
| Flanks / Flank Distance | on / 300 cm | The pack surrounds you |
| Shuns Light | on | Backs off from a flashlight (see above) |
| Opens Doors / Bangs On Doors | off / on (`DoorBang`) | As the hunter |
| Appearance | `DA_AmbusherLooks` | Every zombie look plus Hazmat, Police and Construction |

## Charger (`DA_Charger`)

A rare, heavily mutated enemy (always a mutant body: `DA_MutantLooks`, Bugrimov Maksim's five creatures). Spotting a
player at least Charge Min Distance away, with a clear straight run to them (nothing its width in the way; else it
comes round first), it stops and **roars** (the warning, with three heavy **stomps**), then **charges** in a nearly straight
line, very fast; it runs **dead straight** at where you were when it set off (developer, 2026-11-13: it tracked a sidestep). It **bursts through** unlocked doors (**slammed** open, swinging 7x as fast, as it comes, so it doesn't run into a half-open door; developer 2026-11-13; a locked door stops it with a crash) and **knocks loose props aside** (a door it slams open: its own door slam and a jolt, 2026-11-14). Its running steps are heavy **stomps** of its own (`DA_MutantLooks` → Footstep Sounds, whatever the floor; 2026-11-14) at Footstep Volume 0.3 (developer, 2026-11-15 and 16: too loud) and Footstep Pitch 1.3 (too deep: it sounded heavier than it looks), each as a foot lands (2026-11-16: they didn't line up), cut to 0.5 s so they don't run together; fixed scenery (the test map's crates, walls) still crashes it. Shot in the leg, it slows. So a sidestep makes it miss. A **hit** does Charge Damage (a kill), then it recovers
briefly and fights up close like a hunter. A **miss** (sidestepped: 1.5 m past you, or 25 m run) doesn't stun it
(developer, 2026-11-17): it pulls up for Charge Miss Recover Time and comes back at you. A **wall** stuns it: dizzy on
its feet, then a roar shaking it off, about 6 s, time to run or to shoot it. A wall square ahead (within Wall
Crash Reach) is a **crash** at full tilt, not a slide along it: a heavy thud (one of four), and it **shakes the view** of
players within 15 m (more the closer). Growls now and then.

| Setting | Value | What it does |
|---|---|---|
| Health | 400 | M4: about 3 headshots, 12 body shots |
| Charge Min Distance | 500 cm | Charges at players further than this; closer, it fights like a hunter |
| Roar Time | 1.6 s | Roaring before the charge |
| Roar Retry Delay | 3 s | As the roar ends it checks the run again: blocked (you stepped behind something), it comes round instead and roars again no sooner than this |
| Charge Speed | 540 cm/s | You: 260 unarmed (was 750, 680, then 620: too hard to sidestep; developer, 2026-11-12 and 13) |
| Charge Turn Rate | 0°/s | How much it can turn while charging: none (it was 35, and tracked a sidestep) |
| Charge Tell Sounds / Count | Concrete Land x3 | Its warning, spread over the roar: three heavy stomps (no floor scrape: developer, 2026-11-14) |
| Door Slam Sounds | ChargerDoorSlam | Slamming a door open as it charges: this at the door, with the wall crash's shake, once a door |
| Limb Health / Limp Speed Scale | 150 / 0.75 | A leg shot for 75 or more slows it to 75% (charge too); a mutant's limbs never come off (no pieces) |
| Charge Max Distance | 2500 cm | A charge this long is a miss |
| Charge Damage | 100 | Kills a player at full health (in co-op with a teammate up, that downs them) |
| Stumble Down Time | 4 s | Stunned on its feet after a wall crash, dizzy (Mixamo Dizzy Idle) (plus a 1 s recoil and a 1.3 s roar shaking it off; it used to fall and crawl: developer, 2026-11-16) |
| Charge Recover Time | 1.5 s | Standing after a hit |
| Charge Miss Recover Time | 0.6 s | After a miss: pulling up before it fights on; no stun (developer, 2026-11-17) |
| Wall Crash Effect / Time / Scale | NS_ChargerWallDust / 0 / 1 | A puff of dust out of the wall, the developer's own (`/Game/Enemies/Effects`, 2026-11-17; Vefects' wind dust before it couldn't be seen). Time: how long a looping effect spawns (0 for a one-shot) |
| Wall Crash Sounds | 4 | Door smacks and war club blows from the Sound Effects bundle (`import_charger_crash.py`), one at random |
| Wall Crash Reach | 40 cm | A wall this close in front (square on) is a crash |
| Wall Crash Min Angle | 15° | Its body stopped by a wall met at least this far off parallel is a crash too; shallower, a graze, it slides along (developer, 2026-11-17: at a slant it just stopped) |
| Wall Crash Shake Radius / Shake | 1500 cm / 1.2 (was 0.8) | View shake for players near a wall crash (1 = a heavy blow's jolt) |
| Run Speed / Attack Damage | 180 cm/s / 60 | Up close; slower than a walking player (260) so you can open the distance it needs to charge again (was 300: it kept up; developer, 2026-11-17) |
| Wander Radius | 400 cm | |
| Alert / Idle Sounds | 4 roars / 4 growls | MonsterRoarsAndGrowls |
| Appearance | `DA_MutantLooks` | A behaviour's own looks replace the enemy's |
| Animations | `DA_MutantAnims` | Its own moves (below), replacing the enemy's |

Its moves are the developer's Mixamo mutant clips (`DA_MutantAnims`, 2026-11-16; table below). Rarity is up to
placement (one in the `L_EnemyTest` hall to try).

## Siren (`DA_Siren`)

Barely turned: it still looks like a person (**MetaHumans**, `DA_MetaHumanLooks`: Npc_1, Npc_2 and the downloaded MHC_Advika, MHC_Hannah, Mara1 (Skotukeda) (2026-11-13; no outfits: assembled without clothing); their hair is held on the nearest LOD that really has cards (LOD 3: the grooms say cards for LODs 2-4 but only LOD 3 has any, and held on LOD 2 they were bald; fixed 2026-11-13), since optimized MetaHumans have no strands up close, made by `setup_metahumans.py` from every
MetaHuman assembled into `/Game/MetaHumans`; **blood on their faces** (developer, 2026-11-16: to fit the environment; the MetaHuman Blood Overlay pack, forehead or over the nose, `setup_siren_blood.py`, re-run after `setup_metahumans.py`); the older stand-in, the zombies' clean variants, is `DA_HumanLooks`) and moves with a person's animations (`DA_HumanAnims`). It **waits standing, terrified, or sitting on the floor, dazed**, at random (developer, 2026-11-14: never lying; Mixamo Terrified and Sitting Dazed, `setup_siren_lying.py`), and **beckons** you with a weak wave when you are within 10 m in sight (standing; no sitting wave clip yet), still
**calling for help**, ignoring being seen; come within Siren Distance (and in sight), or shoot it, and it **gets up and
goes for you**, then hunts like a hunter. It **works on one of you at a time** (developer, 2026-11-13: call one player away from the group): with more than one player in sight it waits still and quiet, and calls and beckons someone alone, staying where it is until they are within reach, then springs up (it used to crawl to them; developer, 2026-11-13: crawling is for lost legs). Turned on you and shot down to half its health, it **drops and plays dead** on its back (the one pose kept for that), silent, once, until someone comes close or shoots it again. Real calls for help need a voice pack with words.
**Shots hit the body you see** (2026-11-13): the MetaHuman's own physics asset takes them (its bones, e.g. spine_04), not the hidden zombie body under it, whose male proportions didn't match a woman's look; dead, the ragdoll (the hidden body) takes them.

| Setting | Value | What it does |
|---|---|---|
| Health | 120 | Weaker than a zombie: one M4 headshot, 4 body shots |
| Lies Down | on | Waits in one of its poses: standing terrified or sitting dazed, at random (`DA_HumanAnims` → Lying Poses, not those Playing Dead Only; off: standing in its idle) |
| Siren Distance | 250 cm | A player this close, in sight, sets it off |
| Beckon Distance | 1000 cm | Waiting, a player this close in sight and it beckons them (its pose's Reacting clip) |
| Walk / Run Speed | 140 / 300 cm/s | A person's walk and run |
| Attack Damage | 55 | |
| Idle Sounds / Interval | hurt, scared, sad / 5-10 s | Its calls for help (no words in the voice pack) |
| Alert / Chase Sounds | combat shouts / angry | As it turns, and while chasing |
| Only For One | on | Calls and beckons only someone alone; with more than one player in sight it waits still and quiet |
| Play Dead Health | 0.5 | Turned and shot down to this much of its health, it drops and plays dead, once |
| Stagger Time | 0.5 s | |
| Animations | `DA_HumanAnims` | Mixamo clips retargeted onto the zombie skeleton (`import_mixamo.py`, `/Game/ThirdParty/Mixamo/Zombie`): idle `ZM_Injured_Idle`; attacks three punches; waiting poses (`setup_siren_lying.py`): standing, Terrified, waving a lone player over with Injured Wave Idle; sitting, Sitting Dazed (no wave); both ease into its moves over half a second (no get-up clip). Playing dead only: on its back, Laying Sleeping (lowered 96 cm: recorded on a bed), up with Getting Up 2.0-6.2 s at 2.5x (about 1.7 s). Walk (215 cm/s) and run (615 cm/s): the UE4 template's. |
| Appearance | `DA_MetaHumanLooks` | Npc_1, Npc_2, MHC_Advika, MHC_Hannah, Mara1; the last three have no outfits yet (ENEMY_ASSETS_TODO.md). `DA_HumanLooks` (M04, M02, F02, no gore, clean clothes) is the older stand-in |

## The test map (`L_EnemyTest`)

Each kind has its own room off one long corridor (`build_enemy_test_map.py`, `setup_enemies.py`): the stalker in the
hall (crates and doorways), the charger in a 16 x 8 m hall, four ambushers in a dark room, the hunter pacing in its own
room, the siren standing or sitting in a lit room; the line-up of every body in the side room.

## Shared by all

- **Blows need a clear line** (2026-11-13): an attack starts and lands only with nothing between the two bodies (walls, doors, crates, props) at chest and waist height; within reach but blocked, an enemy goes round to a spot with a clear line, or waits.
- **Footsteps** (2026-11-13, developer): running enemies (over 250 cm/s) make the player's footstep sounds by surface (`setup_enemy_footsteps.py` copies them from `BP_FirstPersonCharacter` onto `BP_Zombie`), each as a foot lands (Foot Bones `foot_l` / `foot_r` coming down within Foot Plant Height, 3 cm, of their lowest after lifting Foot Lift Height, 8 cm; a shuffle still steps once 1.5 strides on; 2026-11-16); a crouched stalker's steps are the quiet crouch sound. Not a noise other enemies hear.
- **Stagger** (2026-11-13, developer): shot, the hunter and stalker stop for 0.4 s and the siren 0.5 s (Stagger Time; at most once a second, Stagger Cooldown) while a hit react plays (zombies: Mixamo's two zombie hits, the start of each, light and heavy, from any side, since 2026-11-14; the siren: a Lyra hit react **on top of** its pose at 1.2x (they are additive clips: played as a whole pose they collapsed the body to a point and shot enemies seemed to vanish; fixed 2026-11-13), **for the side the shot came from** (front, back, left, right); a headshot or a big hit (a quarter of its health) plays a heavy one and lasts 1.8x (Heavy Stagger Scale). `setup_hit_reactions.py`: Lyra's 13 on `DA_HumanAnims`, the 2 Mixamo ones on `DA_ZombieAnims`, and since 2026-11-16 Mixamo's Hit Reaction_Behind for a zombie shot from behind.) Not mid-blow, lying, roaring, charging or down; the charger and ambushers don't stagger (Stagger Time 0).
- **Wounds** (2026-11-13, developer: hits that matter, dismemberment). Damage to a limb (its bones: the head, upper arm down, thigh down; not the neck, which isn't a headshot either: 2026-11-13) adds up; at Limb Health (45: two rifle hits on one arm or leg; was 90, developer 2026-11-13: limbs only ever came off corpses) it **comes off**: gone from the body and its clothes (its bones drawn shrunk into its own joint: the engine's bone hiding stretched a cut leg into a spike, developer 2026-11-16), the skin's wound mask on M02 and F02, the pack's severed piece falling away with physics from the pose it was in, a spray of blood and a gore sound (`DismembermentBody`; the head `DismembermentHead`; 2026-11-14). The **head** comes off only with a killing headshot of Head Off Damage (90; was 120, which range fall-off kept a rifle headshot under). A **leg** hurt by half its Limb Health: it limps (all speeds x0.8; was x0.65, which left a hunter slower than a running player for good, 2026-11-13); a leg off: it drops and **crawls** (`DA_CrawlerAnims`: Mixamo's Zombie Crawl, about 83 cm/s, held in place by its Walk Travel since the clip moves 2 m a loop; at most 1.5x the clip's pace, about 125 cm/s; developer, 2026-11-14: the old crawl barely crept, 8 cm/s). The limb-coming-off sound follows the Gunplay volume. Each **arm** off: its blows do x0.6; with both off it bites (its set's Armless Attacks, 2026-11-16). Standard zombies (M04, M02, F02: `setup_dismemberment.py`) lose limbs; the siren and charger only limp.
- **Each its own** (2026-11-13, developer: a group never moves in step): every enemy rolls its own pace (all speeds) and blow rhythm, each within Variation (12%) either way, and a Reaction Time (0.15 to 0.6 s) between first seeing a player it wasn't after and going for them; shot, or with them within 3 m, it reacts at once.
- **Light and dark** (2026-11-13, developer): an enemy with a Dark Sight Distance makes you out in the dark only that close; lit (your flashlight or weapon light on, or a lamp in the level shining on you with a clear line) as far as it sees. Hunters 10 m, the charger 12 m; ambushers (they react close anyway), the stalker (it always senses you) and the siren don't care. So keeping your light off in the dark keeps you hidden further off, and turning it on gives you away.
- **Voice:** Voice Attenuation `SA_EnemyVoice` (full volume within 1.5 m, falling off like a real sound to silence at 30 m, muffled and quieter through walls; enemy footsteps the same with `SA_EnemyFootsteps`, silent past 20 m: `setup_enemy_sound_distance.py`, developer 2026-11-13: they sounded closer than they were), Voice Volume 1. One voice at a
  time per enemy.
- **Noises they hear** (a noise of loudness L is heard within L × Hearing Range): sprint step 1, walk 0.6, landing 0.8,
  quiet walk (Shift) 0.25, crouch-walk 0.12, gunshot 4. With Hearing Range 1000: a crouch-walk at 1.2 m, a quiet walk
  at 2.5 m, a walk at 6 m, a landing at 8 m, a shot at 40 m, in the open. **Walls muffle** (2026-11-13, developer): each wall, closed door or big prop between cuts how far a noise carries to Wall Muffle (0.35) of it: a shot is heard 14 m off through one wall, 5 m through two; a walk only 2 m through one.
- **Body** (`BP_Zombie`): 200 health unless the behaviour sets one; headshots ×4; ragdoll on death; a corpse goes after 3 minutes (Corpse Life Span), but never with a player within 25 m (Corpse Keep Distance; it looks again every 10 s) (2026-11-13: bodies piled up as the Director sent more).

## Random looks (`DA_ZombieLooks`)

**More bodies** (2026-11-14, developer: variety). The Priest (M09) joins the zombies with its own limb pieces (one body, half as likely as the others). Hazmat (M06), Police (M07) and Construction (M08) have no limb pieces, so only **ambushers** wear them: `DA_AmbusherLooks` (DA_Ambusher's Appearance) is every zombie look plus those three, which can't lose limbs. Each of these ships one outfit (no colour variants).

| Model | Bodies | Material choices | Clothing and hair |
|---|---|---|---|
| M04 (suits) | 8 | body gore 2, head gore 3, suit 7 colours, hair 6 colours | in the body mesh |
| M02 (Starved male) | 2 (two heads) | body gore 4, each head 4 | jeans (4 colours); T-shirt (6), singlet (6) or none; hair or none |
| F02 (Starved female) | 2 (two heads) | body 8, each head 5 | cheerleader top and skirt (15, matching) or lingerie top (8); hair (2) or none |

Each model has a **Weight** (how often it comes up; all 1 now). F02 uses M02's physics asset: her own has collision
off on the torso, so shots went through her. F02 speaks with the male voices for now (no female voice pack yet).
Rebuilt by `setup_enemies.py` on every run (it's the packs' data, not tuning).

## Animations (`DA_ZombieAnims`)

| Slot | Clip | Notes |
|---|---|---|
| Idle | YI `Zombie_Idle_1_v2` | |
| Walk | Zombie Animation Pack `anim_Walk_A` | about 50 cm/s, played faster or slower to match |
| Run | Zombie Animation Pack `anim_Run_A` | about 192 cm/s; Max Run Play Rate 2.5 |
| Calm Walk | YI `ThirdPersonWalk` (UE4 template walk) | A pacing hunter; about 215 cm/s, played slower |
| Roar / Stumble | YI reach; stand-to-crawl, crawl idle, crawl-to-stand | Unused since the charger has its own (below) |
| Crouch / Crouch Walk | Lyra crouch idle / walk | Legs and back only; arms, neck and head stay the zombie's own. Crouch walk 300 cm/s at normal speed |
| Attacks | YI arms swing, hyper attack | Start / hit / end times per attack |
| Armless Attacks | Mixamo Zombie Neck Bite (0.2-1.9 s, the head's lunge at 1.05) | With both arms off (2026-11-16); still at the armless damage |
| Hit Reactions | Mixamo Zombie Reaction Hit, (1), Hit Reaction_Behind | Front light, front heavy, back (2026-11-16) |

### The charger's (`DA_MutantAnims`, 2026-11-16)

All Mixamo mutant clips, retargeted by `import_mixamo.py`; set by `setup_enemies.py`, which also points `DA_Charger` at it.

| Slot | Clip | Notes |
|---|---|---|
| Idle | Mutant Idle | |
| Walk / Calm Walk | Mutant Walking | about 124 cm/s; not in place (176 cm a loop): Walk Travel holds it |
| Run | Mutant Run | about 224 cm/s; not in place (194 cm a loop): Run Travel (new) holds it |
| Attacks | Mutant Punch (blow 0.3 s), Mutant Swiping (blow 1.0 s) | |
| Roar | Mutant Roaring 0.3-2.3 s at 1.25x | Fills its 1.6 s Roar Time: rears back, then roars |
| Stumble | Zombie Reaction Hit (recoil), Dizzy Idle (stunned; Mutant Breathing Idle unused), Mutant Roaring 0.6-1.9 s (shaking it off) | It stays on its feet |

## Not done yet

The stalker dragging a downed player off (needs a pose: ENEMY_ASSETS_TODO.md); creature-specific animations and voices
(a female zombie voice, words for the siren and the hunter); a wall-crash effect; a
difficulty setting for damage. Things the code may get wrong that need a play test first: ENEMY_PLAYTESTS.md.

## The director (2026-11-13)

Paces a level so it never plays the same (developer: calm stretches building to peaks). It watches how hard things are
for you (damage you take, shots you fire, enemies on you) and, after a long calm (45-75 s), brings a **wave** in at a
spawn point out of everyone's sight 15-35 m away: one or two hunters coming for you (most often), an ambusher pack, or a
stalker. At a **peak** it gives you a 25 s breather (no waves, stalkers hold off). At most 12 enemies alive (placed ones count too: ENEMY_PLAYTESTS.md). A hunter it sends comes with its ordinary shout, not the scream (it was called). Settings:
`DA_Director`. Placed in `L_TestFacility` (the generic test facility: every kind of enemy, doors, a locked storage room
with the key in the offices, lit and dark rooms).

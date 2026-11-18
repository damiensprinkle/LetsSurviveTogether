Tactical Horror FPS --- Game Design & Development Plan
Document version: 0.5
Last updated: 2026-11-13 (story decided: section 9)
Status: Early planning / pre-production
Engine direction: Unreal Engine 5
Game name: Unitas (developer, 2026-11-12: "Let's Survive Together" made it sound less serious; the
project files and folders still use LetsSurviveTogether).



1. High Concept
A mission-based, cooperative tactical horror first-person shooter set in
the modern day. Players investigate unusual incidents in dark, dangerous
environments, complete varied objectives, uncover mysteries, and survive
encounters with hostile creatures.

The shooting should feel precise, responsive, and skill-based, taking
inspiration from the weapon handling and mechanical satisfaction of
Counter-Strike 2. The atmosphere, mission structure, exploration, and
supernatural threats take inspiration from tactical horror games such as
Ready or Not, while establishing a distinct identity.

Story update (developer, 2026-11-13): the players are now four
researchers who wake up inside Aegis, the research facility where the
outbreak began, and have to find their way out (section 9). They start
with only a flashlight and find their weapons. Sections below that
describe operators, a staging area, briefings and independent missions
(3, 4.5, 5, 8, 11, 13) are from the earlier direction and are on hold
until they are reworked for this story.

Core design premise: Players are capable and well-armed, but they do
not fully understand what they are entering. Good aim and teamwork can
save them, but neither guarantees safety.
2. Established Design Decisions
These decisions are the current foundation of the project.
  Area                                Decision
  Genre                               Tactical FPS / horror

  Engine                              Unreal Engine 5

  Perspective                         First-person

  Players                             1--4 players

  Multiplayer                         Online and LAN

  Session model                       Player-hosted lobby / listen server

  Campaign structure                  One continuous story in the Aegis facility (2026-11-13; was: independent missions)

  Story direction                     Survival mystery: researchers waking up inside the outbreak (section 9)

  Setting                             Modern day; Aegis, a research facility

  Gunplay                             CS2-inspired, skill-based

  Arsenal                             Limited, carefully balanced

  Mission failure                     Restart the mission after a team
wipe

  Environments                        Varied
Preparation                         None for now (2026-11-13: the players start in their offices
at Aegis; the firing range stays only as an internal testing tool)
Where details are not yet decided, keep them flexible. Do not silently
turn suggestions in this document into fixed requirements.
3. Core Gameplay Loop
1. Prepare: Players meet in a staging area, review the mission
   briefing, and select weapons and equipment.
2. Deploy: The team enters the mission environment.
3. Explore and investigate: Players navigate the location, gather
   information, solve puzzles, and discover threats.
4. Respond: Players use weapons, equipment, teamwork, and
   situational awareness to handle encounters.
5. Complete objectives: The team accomplishes the mission's primary
   and optional goals.
6. Extract: Players reach the extraction point or otherwise satisfy
   the mission's completion condition.
7. Debrief: The game presents mission results, statistics, and
   discoveries before players prepare for another mission.
Missions should be independently playable and have their own goals. They
may still reveal pieces of a larger mystery.
4. Design Pillars
4.1 Skill-Based Gunplay
Shooting should be satisfying even when no horror encounter is taking
place.
- Responsive mouse aiming and movement.
- Predictable, learnable recoil behavior rather than entirely random
  recoil.
- Accurate and consistent hit registration.
- Distinct weapon identities and handling.
- Meaningful headshots and precision.
- Responsive weapon switching and reloading.
- Clear feedback from firing, impacts, and damage.
- Weapon handling should support skill without making the player feel
  sluggish.
The initial goal is CS2-inspired mechanical satisfaction, not a direct
copy of CS2's exact movement or weapon balance.
4.2 Atmospheric Horror
Players should feel vulnerable despite carrying effective weapons.
- Darkness is a gameplay mechanic, not merely a visual treatment.
- Limited visibility creates uncertainty and tension.
- Environmental audio can provide clues about nearby threats.
- Jump scares should be occasional and purposeful, not constant.
- Some threats should be better avoided than fought.
- Not every frightening event needs to be an enemy encounter.
- The use of light should create meaningful choices.
4.3 Cooperative Play
Players should benefit from communicating and coordinating.
- Covering different directions.
- Sharing information about threats and discoveries.
- Helping incapacitated teammates.
- Coordinating equipment and ammunition.
- Working together on selected environmental puzzles.
- Creating situations where players can protect one another.
Do not require every puzzle or objective to have multiple players. The
game should remain workable with fewer than four players.
4.4 Varied Missions
Missions should not all be combat-clearing exercises. Some should
emphasize investigation, exploration, puzzles, survival, retrieval, or
containment.
4.5 Meaningful Preparation
Loadout choices should affect how players approach a mission.
Flashlights and night vision, for example, should offer different
advantages rather than one being a direct upgrade over the other.
5. Preparation Area and Loadouts
On hold (developer, 2026-11-13): with the new story there is no staging
area for now. The firing range (L_FiringRange) stays only as an
internal testing tool. The rest of this section is the earlier
direction.

The preparation area is a consistent physical location between missions,
not just a menu.

Potential interactive stations:
  Station                             Purpose
  Mission briefing                    Review the mission overview,
objectives, known threats, and
available intelligence

  Weapon locker                       Select primary and secondary
weapons

  Equipment locker                    Select flashlight, night vision,
medical supplies, ammunition, and
mission equipment

  Mission map                         Review known layout information or
available intelligence
Deployment terminal                 Start the mission when the team is
ready
Keep preparation reasonably quick. It should provide useful choices
without forcing players to spend excessive time between missions.
5.1 Initial Equipment Concepts
- Flashlight.
- Night vision goggles.
- Medical supplies.
- Additional ammunition.
- Suppressor (developer's addition, 2026-11-04).
- Specialized mission equipment.
Potential distinction between light sources:
- Flashlight: Natural color and useful local illumination, but can
  reveal the player's position.
- Night vision: Better visibility in darkness, but may distort
  colors, perform poorly around bright light, or make threat
  identification harder.
These are design hypotheses to test, not final balance specifications.
6. Gunplay and Initial Arsenal
Weapon behavior should be reusable and separated from individual weapon
configuration.

Initial weapon archetypes:

  Archetype                Intended identity
  M4-style assault rifle   Balanced automatic rifle
AK-style assault rifle   Heavier feel and stronger recoil
MP5-style SMG            Fast handling and close-range effectiveness
Glock-style pistol       Reliable secondary weapon

These are archetypes, not commitments to exact real-world models.

Start with a small number of well-developed weapons rather than a large
collection of shallow ones.
6.1 Weapon Features
- Hip-fire and aim-down-sights.
- Recoil behavior.
- Firing modes where appropriate.
- Reloading and magazine ammunition.
- Weapon switching.
- Weapon-specific handling and movement effects.
- Weapon animations and sound.
- Server-validated firing and damage in multiplayer.
Weapon progression should not create large damage advantages for players
with more playtime. New equipment and attachments should primarily offer
different tactical options.
7. Enemies and Horror Encounters
Enemy concepts should create different gameplay situations. Avoid making
every enemy a bullet sponge.

In the story (2026-11-13, section 9) all enemies are people infected by
the Quorum, and there are five types: Sirens, Ambushers, Hunters,
Stalkers and Chargers (9.3; how each plays is in ENEMIES.md). The
Mimic below is not in the story's list.

Initial conceptual archetypes:
  Archetype               Behavior                Gameplay purpose
  Stalker                 Watches from a distance Psychological tension
and disappears when
approached; may follow
without attacking       

  Ambusher                Hides in ceilings,      Tests awareness and aim
vents, or dark corners
and attacks suddenly    

  Hunter                  Actively pursues        Creates panic and
players and may be      resource pressure
difficult to kill       
Mimic                   Imitates familiar       Creates uncertainty
sounds, movement, or
environmental cues
Not all of these need to appear in the first prototype. Begin with one
basic hostile and one ambush-style enemy.

Ambusher, decided (developer, 2026-11-11): packs, like the creatures in
I Am Legend: they stand together, heads down, until a player comes within
a few metres or makes a sound near them, then the whole pack bursts out.
Each dies to one headshot. They don't patrol.

Charger, decided (developer, 2026-11-12): a rare, heavily mutated enemy.
Seeing a player at a distance it roars, then charges very fast in a
nearly straight line; a hit hurts badly; running into a wall stuns it a long while, time to run
away or to shoot it. A miss doesn't stun it: it pulls up and comes back
(developer, 2026-11-17).

Stalker, decided (developer, 2026-11-11): it lies hidden and unaware
until it hears a player (or one looks at it, 2026-11-12), then follows them more like something hiding
than something chasing: it moves only between places they can't see,
hides when they turn towards it, waits until they look away, and moans
when it is right behind them, just before it strikes.

A sound, distant movement, or brief silhouette in a flashlight beam can
be more frightening than another combat encounter.
7.1 Encounter Direction (developer's notes, 2026-11-04)
- A fairly standard investigative horror game. Some missions may only
  have a handful of enemies. The agents are prepared, but they are not
  really expecting what they find.
- Enemies hit hard and move fast; there will likely never be a lot of
  them at once.
- Generally scary: enemies can creep up behind the player, announced
  only by very subtle audio cues.
- Difficulty settings are needed, mainly for how hard enemies hit.
- Many horror players are not used to tactical, skill-based shooting,
  so the difficulty options and onboarding should make the gunplay
  approachable without removing it.
7.2 Sanity (developer's idea, 2026-11-04)
Each player has a sanity bar. As sanity drops:
- Auditory and visual hallucinations (false footsteps, whispers,
  shapes at the edge of the light).
- Shakier aim: more weapon sway.
Ideas for restoring it (undecided): nicely lit rooms, staying near
other players. Open question: what drains it (darkness, seeing
enemies, a teammate going down). Hallucinations are private: they
play only on the affected player's machine, so teammates see and hear
different things.
Decided (developer, 2026-11-11): an audible heartbeat that gets faster
as sanity drops.
No sanity bar on the HUD. Sanity is shown diegetically, through an
in-world item the player looks at, such as a smartwatch showing heart
rate (higher as sanity drops), or a journal. The item is still to be
chosen. The same item could later carry other information instead of
a traditional HUD (objectives, codes and clues for keypads).
Decided (developer, 2026-11-17), built:
- Drains: darkness (no lamp on you; your own flashlight only slows
  it) and being alone (far from every teammate; never when playing
  solo).
- Restores: lit rooms, being near a teammate, and calm (a while with
  nothing draining it).
- Shown by the heartbeat alone for now (no item); near the bottom
  the edges of the view darken, as if about to pass out, swelling
  with each heartbeat.
- Hallucinations are sounds: fake footsteps coming closer, and the
  enemies' own sounds (a stalker breathing or moaning behind you,
  hunter shouts and screams, door bangs, an ambusher shriek, a siren
  crying, a charger's growl and stomps), the lower the sanity the
  more often and the more alarming.
- At zero: a fake enemy rushes the player and vanishes as it reaches
  them (no harm). Shot, it dissolves into a puff of smoke.
8. Mission Design
Missions should be built from reusable objective types and data rather
than mission-specific hardcoded logic.

Potential objective types:

  Objective type   Example
  Explore          Reach a designated area
Interact         Activate a generator
Collect          Retrieve an artifact or sample
Investigate      Find evidence or records
Eliminate        Defeat a specific threat
Survive          Stay alive until a timer or event completes
Puzzle           Solve environmental clues or a mechanism
Escape           Reach an extraction point
Rescue           Locate and escort a missing person
Containment      Activate systems to contain a threat

Objectives can have dependencies. Example:

Restore power -> Unlock laboratory -> Retrieve sample -> Extract

Missions can combine several objective types and include optional
discoveries.
8.1 Mission Archetypes
- Investigation: Explore a location, locate records, and discover
  what happened.
- Exploration / maze: Navigate a complex environment and find a
  route or missing team.
- Survival: Stay alive until extraction becomes available.
- Containment: Activate systems to prevent a threat from escaping.
- Retrieval: Recover an object from a dangerous location.
- Rescue: Find and escort a missing person.
These are reusable mission patterns, not separate game modes.
9. Story and Setting
Decided (developer, 2026-11-13). This replaces the earlier direction (a
specialist incident-response team visiting unrelated incidents across
the country; kept in 9.6 for the record). The players are not
operators: they are four researchers who wake up inside the facility
where the outbreak began, and the game is their way out of it.

9.1 The Facility and the Organism
The facility is called Aegis. It was researching a biological organism
living on Hydnellum peckii (the Devil's Tooth fungus) that let the
fungus "self-heal".

The research looked promising:
- Damaged tissue repaired itself quickly.
- Cancerous cells underwent apoptosis (cell death).
- Researchers observed unusual cellular growth patterns.
But it was not repairing tissue or killing cancer cells. It was
reorganising them. Over time the researchers found it could influence
the nervous system, adapt to different hosts and even communicate over
long distances. They called it "The Quorum" (after quorum sensing):
several specimens appeared to respond to each other, almost as though
they were communicating. It uses living tissue to repair itself,
communicate and spread.

9.2 The Outbreak
- The mouse experiments went very well, and Aegis was granted human
  trials. Patients were kept in containment cells; Aegis was careful
  (it had already had a containment breach at a sister facility), so
  procedures were strict.
- The first patient, Liam Anderson, had stage IV lung cancer and three
  months to live. He was declared cancer-free after 5 days and stayed in
  containment with no symptoms.
- His results were so good that 13 more patients with similar outlooks
  were brought in. One after another, the outcomes were great.
- After 30 days all 14 were let out of containment and allowed into the
  cafeteria. Everything was normal (eating, talking, laughing) until,
  in one moment, they all stopped. All 14 stood up at the same time and
  charged the researchers, attacking, killing and infecting them.
- Aegis went into high-security lockdown and most of the patients were
  locked in the cafeteria. It held for only minutes. As the doors
  locked, the infected looked over the dead and "chose" who would join
  them. Not everyone: it was very particular. After a few minutes the
  chosen stood up and joined them.
- One of the newly infected began to mutate and grow into something
  that no longer looked human, strong enough to break down doors and
  break containment (the first Charger). Idea: finding security
  footage of this (the developer doubts the animation is doable).
- The facility fell into chaos. A few researchers locked themselves
  away to work on a cure and study the infected.

9.3 What the Researchers Learned About the Infected
The infected work together, but some behave differently (the enemy
types of section 7 and ENEMIES.md):
- Sirens: look the most human, speak English, indistinguishable from
  anyone else.
- Ambushers: stand together in small groups and don't react unless you
  are close.
- Hunters: wait for their moment and charge you down.
- Stalkers: seem to enjoy creeping around; they walk behind you and
  make little moans before attacking (2026-11-14: they don't speak or
  whisper; maybe a sinister laugh).
- Chargers: mutated, no longer human-looking; they use brute force.

9.4 The Players
Four researchers, one per player:
- Dr Fields: Lead Researcher.
- Dr Hayes: Medical Researcher.
- Dr Cross: Research Biologist.
- Dr Shaw: Research Oncologist.

9.5 Opening Sequence
Introduction (each player wakes in their own office):
- The player wakes dazed in a small office with no windows. A small
  scuffle happened there (chair flipped over, papers on the floor),
  nothing dramatic. The fluorescent light above is blinding; its
  flicker almost eases the eyes. On the desk: a name plate that can be
  inspected (Dr Fields), a single flashlight and a key. The only other
  thing is one door out.
- The other players wake in similar offices next door (not the same
  room), one each: Dr Hayes, Dr Cross, Dr Shaw. Each office door is
  locked, so only the player inside can open it (with the key on their
  desk).
Hallway and waiting room:
- The players step out into a dimly lit hallway, needlessly long, with
  only a handful of "motivational" posters on the walls.
- It leads to a small waiting room: 8 to 12 chairs and a receptionist
  desk, pitch black. Behind the desk the players find a pool of blood
  and drag marks to a nearby elevator, the only way out.
Elevator:
- Calling the elevator, a distant roar is heard. It has a single
  button: "Lab".
- An audio tape lies on the elevator floor: a doctor telling an unknown
  associate about a breakthrough with Hydnellum peckii (Devil's Tooth)
  as a possible cure for cancer. It plays for about the length of the
  ride; the lights flicker and the elevator shakes as it goes down.
The lab:
- As the doors open and the players walk out, footsteps are heard
  scurrying away, and in the distance someone cries for help.
- The laboratory is fairly standard. TODO: detail how the lab looks.
- Following the cries, the players find a guard: someone lying on the
  floor of a utility closet who gestures for them to come closer. As
  the player approaches, the guard stands up and thanks them; they
  thought they were all alone. TODO: this conversation should be a
  cutscene. Eventually the guard attacks, trying to bite the player
  (the cutscene can be button mashing or similar); the player fights
  back, perhaps hitting them hard in the head with the flashlight. As
  the cutscene ends the player sees the guard dropped a handgun and
  picks it up. (The guard is a Siren: see 9.3.)
- The closet is the only other room in the lab. The only way on is a
  red door with a 4-digit keypad. TODO: a simple puzzle that shows how
  to get the numbers.
Security checkpoint:
- The red door leads to a security checkpoint, the first enemy the
  players have to fight: Hunters charge them as soon as they step
  inside, scaled by player count (1 for 1 to 2 players, 2 for 3, 3 for
  4).
- The checkpoint leads to the main research corridor, the central hub
  of the main research wing. Areas off the corridor:
  - Administration.
  - Utilities.
  - Containment.
  - Research.
  - Classified Area.
  (To be designed.)

9.6 Earlier Direction (superseded 2026-11-13)
Kept for reference. The developer's notes of 2026-11-04: the players
were a specialist incident-response team investigating strange
incidents across the country (a power plant that loses contact, a
subway station evacuated after passengers attack one another, a rural
village that stops communicating, a research facility that reports a
containment failure then deletes its records), thought to be isolated
but eventually found to be connected; the infected could behave
normally until a greater host triggers them, some of the newly
infected still speak English, and attacks might appear coordinated.
The infected and their "greater host" survive in the new story as the
Quorum and its Sirens.
10. Multiplayer Design
10.1 Established Direction
- Support 1--4 players.
- Support both online and LAN play.
- Use player-hosted sessions.
- A player creates a lobby and friends join.
- Keep solo play possible where practical.
- Use a listen-server model initially.
Public matchmaking and dedicated servers are not initial requirements.
10.2 Networking Principles
- The server is authoritative for enemy AI, damage, mission
  objectives, and important interactions.
- Replicate relevant player and gameplay state to clients.
- Player movement should use Unreal's networking capabilities.
- Mission state should be shared consistently.
- Session-provider-specific code should be isolated behind a session
  subsystem.
- Test with multiple clients early rather than postponing multiplayer
  validation.
Epic Online Services (EOS) is a candidate for online sessions, subject
to platform, distribution, and current Unreal integration requirements.
LAN discovery and online sessions should be kept behind a common session
interface where practical.
10.3 Downed State and Team Wipe
Proposed behavior:
- A player who takes lethal damage becomes incapacitated for a limited
  period.
- Teammates can revive them.
- If the entire team is incapacitated, the mission restarts.
Decided (2026-11-09): a downed player bleeds out after 30 seconds unless
revived; enemies can finish off downed players; a downed player cannot
shoot but can crawl slowly; there is no limit on how many times a player
can be downed. Whether particular mission types use different failure
rules remains to be designed.
Decided (2026-11-12): no healing items. Players have 100 health and heal
back by themselves: after 5 seconds without being hurt the blood on screen
clears and health returns slowly (about 20 seconds from nearly dead to
full). Enemies hit hard instead: most blows take more than half your
health, the charger's charge kills, and ambushers hit for less since they
come in packs.
11. Mission Lifecycle
On hold (2026-11-13): written for independent missions with a staging
area; to be reworked for the continuous Aegis story (section 9).
Every mission should use a consistent lifecycle:
1. Briefing.
2. Preparation and loadout selection.
3. Deployment.
4. Active mission.
5. Objective completion.
6. Extraction or mission completion.
7. Debrief.
8. Return to preparation.
The mission framework should support mission start, objective
progression, success, failure, restart, and return to the staging area.
12. Progression and Rewards
Keep progression simple during the prototype.

Potential progression systems:
- Weapon unlocks.
- Weapon attachments.
- Additional equipment.
- Cosmetic customization.
- Mission ratings.
- Optional objectives.
- Collectible evidence and documents.
- Access to additional missions.
Avoid large permanent damage upgrades. Maintain a relatively level
playing field between new and experienced players.

12.1 Found Equipment (developer's direction, 2026-11-04)
Equipment and extra guns start locked. They are found during missions
and unlocked permanently once that mission is completed:
- Hand flashlight: the exception, given right away (found early in
  the first mission).
- Weapon light (tactical flashlight on the gun).
- Suppressor.
- Additional guns.
- Flashlight blacklight (UV) mode: an unlock that reveals hidden clues
  (writing, traces) only visible under it, used for secrets later on.
In co-op, each player has to pick up a find themselves to unlock it.
Finds can sit off the main path: the story may never require opening
a locked door, but the room behind it may hold an upgrade. Locked
doors can always be opened somehow, which rewards exploring. For now
they open with keys found in the mission or keypads (codes found as
clues).

A debrief may include:
- Mission completion status.
- Time taken.
- Enemies encountered and eliminated.
- Objectives completed.
- Optional discoveries.
- Player deaths and revives.
13. First Playable Prototype
The first vertical slice should be one small, complete mission lasting
approximately 10--15 minutes.

Environment: An abandoned modern research facility.
Update (2026-11-13): the natural first slice is now the opening of the
story (9.5): offices, hallway, waiting room, elevator, lab, the Siren
guard, the keypad door and the security checkpoint's Hunters. The
preparation area, briefing, loadout selection and debrief below are on
hold.

Players: 1--4 cooperative players.
Weapons
- One assault rifle.
- One pistol.
- Recoil and weapon handling.
- Reloading and ammunition.
- Weapon switching.
Equipment
- Flashlight.
- Night vision.
Enemies
- One basic hostile creature.
- One ambush-style creature.
Objectives
- Restore electricity.
- Find a keycard.
- Unlock a restricted area.
- Survive an encounter.
- Extract.
Supporting Systems
- Preparation area.
- Mission briefing.
- Basic loadout selection.
- Cooperative player spawning.
- Downed and revive system.
- Mission restart.
- Debrief screen.
The prototype should prioritize functional gameplay over polished
visuals. Greybox environments, placeholder weapons, and simple lighting
are acceptable.

The most important early validation is whether the rifle feels
satisfying to shoot in a dark environment while a threat is trying to
kill the player.
14. Unreal Engine Technical Architecture
14.1 Architecture Approach
Use a C++ foundation with Blueprint-driven content.

Use C++ for reusable gameplay systems, networking, weapon logic, and
core frameworks. Use Blueprints for content configuration, weapon
variants, enemy variants, interactions, animations, and mission-specific
behavior.

Use Data Assets for configurable values instead of scattering hardcoded
weapon and mission settings throughout the codebase.

Avoid building a single enormous Blueprint or C++ class. Keep systems
modular and give AI coding agents small, bounded implementation tasks.
14.2 Unreal Gameplay Framework
  Unreal class       Intended responsibility
  GameInstance       Persistent game-wide systems
GameMode           Server-authoritative game rules
GameState          Shared, replicated mission state
PlayerState        Per-player state
PlayerController   Player input and UI ownership
Character          Physical player and movement
Actor Components   Reusable gameplay systems
Subsystems         Session and other service management

Remember that GameMode is server-only, while GameState is replicated to
clients. For example, the server determines that an objective is
complete and updates shared state for all players.
14.3 Proposed Source Layout
This is a starting organization. Adjust it as implementation reveals
actual dependencies.
Source/
└── TacticalHorror/
    ├── Core/
    │   ├── THGameInstance
    │   ├── THGameMode
    │   ├── THGameState
    │   └── THPlayerState
    │
    ├── Player/
    │   ├── THCharacter
    │   ├── THPlayerController
    │   └── Components/
    │       ├── HealthComponent
    │       ├── InventoryComponent
    │       └── InteractionComponent
    │
    ├── Weapons/
    │   ├── THWeaponBase
    │   ├── WeaponComponent
    │   ├── WeaponData
    │   └── WeaponTypes
    │
    ├── Equipment/
    │   ├── Flashlight
    │   ├── NightVision
    │   └── EquipmentData
    │
    ├── AI/
    │   ├── THEnemyBase
    │   ├── AIController
    │   └── Behavior/
    │
    ├── Missions/
    │   ├── MissionManager
    │   ├── MissionData
    │   ├── MissionObjective
    │   └── ObjectiveTypes/
    │
    ├── Interaction/
    │   ├── InteractableBase
    │   ├── Door
    │   └── Pickup
    │
    ├── Multiplayer/
    │   ├── SessionSubsystem
    │   └── NetworkUtilities
    │
    └── UI/
        ├── HUD
        ├── MainMenu
        ├── Lobby
        └── MissionDebrief
14.4 Initial System Responsibilities
  System            Responsibility
  Player            Movement, aiming, health
Weapon            Firing, recoil, reloads, ammunition
Equipment         Flashlight, night vision, medical items
Inventory         Equipped items and ammunition
Enemy AI          Perception, behavior, attacks
Mission Manager   Objectives and mission state
Interaction       Doors, switches, pickups
Multiplayer       Sessions, replication, player spawning
Game Flow         Briefing, deployment, extraction, restart
14.5 Weapon Architecture
Separate weapon behavior from weapon configuration.

Weapon base class (C++):
- Firing.
- Reloading.
- Ammunition state.
- Weapon state.
- Server validation.
- Hit detection.
Weapon Data (Data Asset):
- Damage.
- Fire rate.
- Recoil parameters.
- Magazine capacity.
- Reload duration.
- Movement effects.
- Sound references.
Weapon Blueprint:
- Mesh.
- Animations.
- Muzzle effects.
- Shell casings.
- Visual customization.
A new weapon should be addable through configuration and content assets
without rewriting the core firing system.
15. Development Roadmap
Milestones should produce playable, testable results. Do not build all
menus, content, or progression systems before validating movement and
gunplay.
Milestone 1 --- Project Foundation
Goal: A working Unreal project with a basic multiplayer foundation.
- [ ] Create or select the Unreal project.
- [ ] Establish C++ and Blueprint conventions.
- [ ] Set up Git repository and folder structure.
- [ ] Configure Enhanced Input.
- [ ] Implement first-person character.
- [ ] Implement basic movement.
- [ ] Create listen-server multiplayer.
- [ ] Test two players connecting.
Exit criteria: Two players can connect to a host and move around the
same test level.
Milestone 2 --- Gunplay
Goal: Make shooting feel excellent.
- [ ] Weapon base class.
- [ ] Assault rifle.
- [ ] Pistol.
- [ ] Recoil behavior.
- [ ] ADS and hip-fire.
- [ ] Reloading and ammunition.
- [ ] Weapon switching.
- [ ] Weapon animations.
- [ ] Replicated firing and damage.
Exit criteria: Two players can use weapons in a test range and
observe consistent firing and damage behavior.
Milestone 3 --- Equipment and Atmosphere
Goal: Establish the basic horror experience.
- [ ] Flashlight.
- [ ] Night vision.
- [ ] Health and damage.
- [ ] Basic audio system.
- [ ] Environmental lighting.
- [ ] Interaction system.
- [ ] Doors and pickups.
Exit criteria: Players can navigate a dark test environment using
the available equipment and interact with basic objects.
Milestone 4 --- Enemies and AI
Goal: Introduce threats.
- [ ] Enemy base class.
- [ ] AI perception.
- [ ] Patrol behavior.
- [ ] Investigate sounds.
- [ ] Chase and attack.
- [ ] Death and hit reactions.
- [ ] Network replication.
Exit criteria: A server-controlled enemy can detect and engage
players in a multiplayer test.
Milestone 5 --- Mission Framework
Goal: Create reusable missions.
- [ ] Mission data assets.
- [ ] Objective framework.
- [ ] Objective dependencies.
- [ ] Extraction.
- [ ] Mission failure and restart.
- [ ] Mission debrief.
Exit criteria: A test mission can progress through multiple
objectives, complete, fail, and restart.
Milestone 6 --- Preparation and Multiplayer Experience
Goal: Connect gameplay systems into a complete session flow.
- [ ] Main menu.
- [ ] Host LAN game.
- [ ] Host online game.
- [ ] Join session.
- [ ] Lobby.
- [ ] Loadout selection.
- [ ] Mission briefing.
- [ ] Deployment.
- [ ] Return to preparation area.
Exit criteria: Players can host or join a session, prepare, deploy,
and return to the preparation area.
Milestone 7 --- First Complete Mission
Goal: Deliver a playable 10--15 minute cooperative horror
experience.
- [ ] Build research facility.
- [ ] Implement mission objectives.
- [ ] Add two enemy types.
- [ ] Add scripted and dynamic scares.
- [ ] Implement extraction.
- [ ] Test full mission with four players.
Exit criteria: A team can complete or fail the mission from
beginning to end, with objectives and outcomes synchronized for all
players.
16. AI-Assisted Development Workflow
Use an AI coding agent to work against the Unreal C++ project, with the
developer reviewing and testing the result in Unreal Editor.

Recommended feature workflow:
1. Write a small feature specification and acceptance criteria.
2. Ask the coding agent to inspect the existing architecture before
   changing files.
3. Have it implement one bounded feature.
4. Compile the project.
5. Configure required assets and Blueprints in Unreal Editor.
6. Test locally and with multiple clients when relevant.
7. Review changes and commit the working feature.
Project Documentation
Maintain these documents as the project grows:
  File                                Purpose
  GDD.md                            Overall game design

  TECHNICAL_ARCHITECTURE.md         Unreal architecture and conventions

  ROADMAP.md                        Milestones and priorities

  TASKS.md                          Individual development tasks
AI_INSTRUCTIONS.md                Instructions and constraints for
coding agents
AI Agent Guardrails
- Read existing code before proposing or implementing changes.
- Follow the established architecture and naming conventions.
- Avoid introducing unnecessary dependencies.
- Do not duplicate existing systems.
- Keep gameplay logic and content configuration appropriately
  separated.
- Account for server authority and replication when implementing
  multiplayer features.
- Do not claim a feature works without compiling and testing it.
- Keep changes focused and explain any required Unreal Editor setup.
- Update documentation when architecture or established design
  decisions change.
17. Open Decisions
These are deliberately unresolved and should be addressed when they
become relevant.
  Topic                               Current status
  Game title                          Unitas (developer, 2026-11-12)

  Investigative organization and lore DECIDED 2026-11-13: Aegis facility, the Quorum (section 9)

  Exact monster designs and mythology The Quorum and its five infected types (9.3, 7); looks to design

  Exact weapon roster                 Initial archetypes proposed

  Health and damage model             100 health, heals back after 5 s unhurt; no healing items

  Ammunition economy                  Undecided

  Revive duration and rules           30 s bleed-out, no down limit (10.3)

  Session provider                    EOS candidate

  Voice chat                          Proximity voice plus walkie-talkie;
                                      both attract enemies (radio more)

  Player progression and unlock       Undecided
economy                             

  Mission rating system               Proposed, not finalized

  Art direction                       Dark modern environments; exact
visual style undecided
  Existing Unreal project vs. clean   DECIDED 2026-10-30: built on
  project                             FPS_Horror_Template (UE 5.7 copy)
                                      plus a new C++ module. See
                                      TECHNICAL_ARCHITECTURE.md.
18. Immediate Next Steps
Done 2026-10-30: project chosen, engine confirmed as UE 5.7.4, C++
foundation (health, weapons, sessions) added and compiling.
See TASKS.md for what is next.

First major gameplay target: A responsive rifle that feels
satisfying to shoot in a dark environment, with a second player
connected to the same session.

The first prototype should be functional before it is beautiful.

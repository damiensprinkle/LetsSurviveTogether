# Roadmap

Milestones 1–6 and 9 come from GDD.md section 15 (goals and exit criteria); 7 (loadout zone) and 8 (story) were added by the developer. This file tracks status and the concrete work under each.
Day-to-day details and "what was just done" live in TASKS.md. Status as of **2026-11-09**.

Legend: ✅ done · 🟡 built, needs play-testing in the editor · 🔧 partly done · ⬜ not started · ❓ needs a design decision first

---

## Now: the next few sessions, in order

1. ✅ **Content cleanup** (2026-11-04/05, see *Content pipeline*): deleting, archiving, the folder moves and the animation renaming are done. Left from it: a new-gun checklist.
2. 🔧 **Automated testing** (2026-11-05, TESTING.md): 19 tests, about 200 s (`Scripts/run_tests.ps1`): ✅ unit, weapon data and content tests; ✅ functional tests in a play session (fire, full auto, reload, switching, both lights, shot damage); ✅ a multiplayer test (listen server + client: shots, reloads, weapon switch, both lights seen by the other player). They found and fixed two bugs: bodies couldn't be hit by bullets, and a crash in the third-person animation. ⬜ Later: screenshot tests once the HUD look is settled; a test per new feature (doors, downed, hit feedback, enemies, friendly fire).
3. ✅ **Two-player play test** (Milestone 1 exit; two PCs, 2026-11-09): host from the main menu, join from a second game instance (or a second PC), go through the checklist in TASKS.md. Includes hearing each other's footsteps, flashlights and weapon lights, Leave Game.
4. 🔧 **Test range** (2026-11-05: the Fab firing range imported as `Maps/L_FiringRange`, walkable, lit with `BP_OverheadLight`, with a ceiling, the template door and an empty room behind it; next spray targets; see TASKS.md *Where we left off*) (an early piece of Milestone 7's firing range): a small map with a grid wall at known distances and a few targets, to check the M4A4 spray against CS2 and tune recoil. Separate from the tests' own map (`/Game/Tests/Maps/L_FunctionalTests`).
5. 🔧 **Hit feedback**: ✅ bullet holes by surface; ⬜ hit-confirm audio cue, blood.
. **Enemy test map**: a small dark level with a navigation mesh for the first enemies (Milestone 4), also Milestone 3's dark test map.
8. **Settings menu, much better** (developer, 2026-11-09): rebindable keys first (real input assets), then graphics; see Milestone 6.

Done recently: GitHub backup (2026-11-03), main menu with Host / Join (hosting play-tested), crosshairs and ammo HUD, flashlights, third-person reload magazine, CS2 M4A4 spray.

---

## Milestone 1: Project Foundation ✅ (complete, developer, 2026-11-09)
- ✅ Project: FPS_Horror_Template (UE 5.7) with C++ module `TacticalHorror`
- ✅ C++ / Blueprint conventions (TECHNICAL_ARCHITECTURE.md)
- ✅ Git with LFS, backed up to GitHub (`origin/main`; the local branch is `master`, pushed with `git push origin master:main`)
- ✅ Enhanced Input (template context + runtime weapon context)
- ✅ First-person character on `ATHCharacter`; CS-style movement (walk, quiet walk on Shift, crouch)
- ✅ Listen-server multiplayer (GameMode / GameState / PlayerState, `UTHSessionSubsystem`)
- ✅ **Exit:** two players connect to a host and move around the same level. Hosting and joining through the menu were checked off-screen between two game instances on one PC (2026-11-03); on two PCs with a packaged build (`Scripts/package.ps1`) the developer reported things working as expected (2026-11-09).
- ✅ Main menu (`/Game/Maps/MainMenu`, the game's start map): Host Game, Find Games with Join per game, Settings, Quit, via `UTHSessionSubsystem` (LAN now, Steam-ready). Hosting play-tested 2026-11-04 (input after hosting fixed). Plain placeholder look until a Widget Blueprint replaces it. ⬜ Joining from a second instance in play.
- 🟡 In-game **Leave Game** in the settings panel (2026-11-03): back to the main menu; a host leaving sends everyone back

## Milestone 2: Gunplay
**Weapons**
- ✅ Weapon base class, data-driven (`UTHWeaponData`)
- 🟡 M4 (full auto; **B** switches to semi, tried and kept 2026-11-04) and Glock 17, first-person arms and animations from the Fab packs
- 🟡 Recoil pattern, view kick and recovery · ADS and hip-fire · reload and ammo · weapon switching
- 🟡 Server-validated firing and damage; fire rate capped for clicking too (fixed 2026-11-03)
- 🟡 Shell casings with collision, bounce sounds, metal material
- 🟡 Muzzle flash (light and quads, player setting for brightness)
- 🟡 M4 spray pattern set to CS2's M4A4 (2026-11-04) from the developer's measured bullet positions (`Scripts/Tools/set_m4_spray.py`). ✅ Checked by the developer in the range (2026-11-09): good as it is (`ViewKickFraction` 0.5)

**Third person (what other players see)**
- ✅ Body animation in C++: directional walk / crouch, aim pitch, fire / reload / equip actions (play-tested by the developer)
- ✅ Weapon placed from the head; hands IK'd onto the rifle; pistol hands and fingers from its animations (hands posed by the developer)
- ✅ Lowered carry when idle, raised on aim / shot / draw (7 s hold); draw animation with the gun revealed in the hand
- ✅ Reload end: hands glide back onto the gun over the last second (`ReloadBlendOutTime`) instead of hanging under it
- ✅ **Third-person reload magazine** (2026-11-04): the old magazine drops out of the gun and falls to the floor (physics, removed after 30 s), a fresh one appears in the left hand at the waist and is seated. Magazine meshes cut from each gun's rig (`setup_magazines.py`); timing per gun on the weapon data (M4 tuned in play). ⬜ A magazine-drop sound (`MagazineDropSound`), ⬜ maybe the first-person view drops one too.
- ⬜ Turn-in-place (feet slide when turning on the spot)
- ⬜ Male rifle walk set (current rifle walks are female `AS_F_*`); pistol crouch; slower crouch walks
- ⬜ Downed / dead body animation (see Milestone 3, Downed)
- ⬜ Small aim correction for the raised pistol (it points where the animation's hand points)

**HUD and feedback**
- ✅ **Custom crosshairs** (2026-11-03, template dot fixed 2026-11-04): 8 styles (the developer's images), picked in Settings with size and R / G / B sliders, saved per player; hidden while aiming down sights; replaces the template's dot but keeps its interaction hand. ⬜ Later, if wanted: spread that grows with `GetCurrentInaccuracy()`, a default style per weapon, opacity.
- ✅ Ammo counter (2026-11-03): magazine / reserve, RELOADING, red when low (no fire-mode label: the developer doesn't want fire-mode switching shown). ⬜ Later: a "press R" hint when empty, if wanted.
- ⬜ **Hit confirmation as an audio cue, no on-screen hit marker** (developer's choice): a short sound for the shooter only when the host confirms the hit, with distinct sounds for headshots and kills
- ⬜ **Blood splatter** on hits to players and enemies: a Niagara burst at the hit point, blood decals on nearby walls and floor, and a damage sound. Played on every machine from the existing shot multicast. A player setting turns blood down or off.
- 🔧 **Bullet holes and impact effects by surface:** (2026-11-09: decals by surface done with the Fab pack, `DA_ImpactSurfaces`; levels still need Physical Materials, and particles, sounds, a decal cap and fade-out are still to do.) Physical Materials (concrete, metal, wood, glass, flesh, dirt) on level materials, each picking decal, particles and sound. Candidate decal pack on Fab. Cap decal count and fade them out.
- ⬜ Damage direction indicator, low-health screen effect

**Input**
- ⬜ Real input assets (`IA_Fire`, `IA_Aim`, `IA_Reload`, …) in `IMC_Weapons`, so keys are rebindable from the settings menu

- ⬜ **Exit:** two players use weapons in a test range and see consistent firing and damage

## Content pipeline (between Milestones 2 and 3)
Needed before more guns: there are many animation assets from several packs (Lyra, Sample Animation Pack, UE template, Fab weapon packs) with pack-specific names, and several have been edited.

- 🔧 **Cleanup (2026-11-04):** inventory of what the game uses (`content_inventory.py`); deleted MetaHumans, the UE template leftovers (Variant_Horror, FirstPerson, Input), the Fab M4 / G17 copies, the placeholder guns and importer leftovers (`cleanup_delete.py`; all in git history); ✅ unused animations to `_Archive/Animations`; ✅ packs to `ThirdParty/<Pack>`, the horror template to `Template/` (moved in the editor 2026-11-05) (`cleanup_reorganize.py`).
- ✅ **Naming and folders** (2026-11-05, `organize_animations.py`, checked by the test `Data.AnimationNaming`; rules in ANIMATION_GUIDE.md):
  - first person: `Content/Weapons/<Gun>/Animations/A_FP_<Gun>_<Action>`, e.g. `A_FP_M4_Reload_Empty`
  - third person: `Content/Animations/ThirdPerson/<Gun>/A_TP_<Gun>_<State>_<Action>`, e.g. `A_TP_M4_Lowered_Walk_F`, `A_TP_G17_Raised_Idle`, `A_TP_M4_HandPose`
  - body sets: `DA_Body_M4`, `DA_Body_M4_Lowered`, `DA_Body_G17`, `DA_Body_G17_Lowered`, `DA_Body_HandFlashlight`, `DA_Body_Unarmed` (one per gun, so each gun can be tweaked)
- ✅ The 71 animations the game plays were copied out of the packs (which stay whole) into those folders, the data assets, aim offsets and scripts repointed; our own originals (M4 / G17 first-person imports, the retargeted pistol idle, the old per-type sets) deleted. No redirectors were needed.
- ⬜ **New-gun checklist** in ANIMATION_GUIDE.md: import → weapon data → first-person tuning → body sets (raised / lowered) → hand pose → shells → sounds → test.
- ⬜ Decide how `setup_body_anims.py` stays the source of truth versus hand-edited assets (it overwrites values; today the developer's numbers are copied into its tables).

## Milestone 3: Equipment and Atmosphere
- 🟡 **Flashlights** (2026-11-04): the hand flashlight (found in the first mission, then key 3, bright, inspected on pickup, left mouse on / off) and the weapon light (upgrade, under the barrel, dimmer, L). Both replicated; the template's L flashlight is replaced. Draw and the G17 light placement play-tested. ⬜ Check the Demo pickup and inspect in play, beam brightness in the dark, and both lights with two players; ⬜ the tactical light's material may not use its textures (they show as unused in the content inventory); ⬜ Unlock the weapon light from a mission find once missions exist (Milestone 5); ⬜ **blacklight (UV) mode** for the hand flashlight, an unlock that reveals hidden clues (a material that shows only under the UV beam); ⬜ gloved first-person arms for the flashlight to match the guns; ⬜ maybe a battery.
- 🔧 Interaction and pickups: template versions work single-player; **replication not verified** (doors are done, above). Convert what breaks into replicated components. (Night vision moved to *Later*.)
- ⬜ **Doors:** the template already has keyed locks, keypads and a valve wheel (developer, 2026-11-05): use those, made server-authoritative (open / close, replicated state, sound), can't close on a player; open speed / push direction; locked variants for missions, opened by **keys found in the mission or keypads** (codes found as clues) for now (decided 2026-11-04). Optional third-person hand reach when opening (an additive arm animation).
- 🔧 **Downed and revive.** Built in code, not yet play-tested: at 0 health a player goes down and bleeds out after 30 s; holding **E** for 3 s on them revives at 30% health; when everyone is down the mission restarts. Still to do:
  - ✅ Rules (developer, 2026-11-09): bleed out after **30 s**; **enemies can finish off** downed players; a downed player **cannot shoot** but **can move slowly** (crawl, `DownedSpeedMultiplier` 0.2, already in code); **no limit** on how many times a player can go down.
  - ⬜ Block firing, reloading, aiming and switching weapons while downed (not done in code yet)
  - ⬜ Downed body pose and crawl for others to see; the downed first-person view (camera low, desaturated, heartbeat)
  - ⬜ UI: bleed-out timer for the downed player, a revive prompt and progress bar for the reviver, a teammate-down marker
  - ⬜ Revive animation for both players (kneel / help up); cancel on damage or moving away
  - ⬜ Death: spectate a teammate until the mission ends or a respawn point
- 🟡 Health and damage (`UTHHealthComponent`), friendly fire switch on the GameMode
- 🔧 Audio: footsteps by movement mode, gun foley, settings menu volume sliders. ⬜ Footsteps by surface (Physical Materials, shared with bullet impacts), reverb zones, an ambience bed
- ⬜ **Sanity** (GDD 7.2): a per-player sanity value (replicated to its owner); low sanity adds weapon sway and local-only hallucinations (sounds, shapes); restored by lit areas and nearby teammates (❓ drains and restores to decide). **No HUD bar:** shown on an in-world item, e.g. a smartwatch on the wrist with heart rate (raise the arm to look at it, first person and third person), or a journal (❓ which item). Tuning in a data asset.
- ⬜ Environmental lighting pass for a dark test map
- ⬜ **Exit:** players navigate a dark test map with their equipment and interact with objects

## Milestone 4: Enemies and AI
- ⬜ `ATHEnemyBase`: the capsule ignores the Weapon channel and the mesh takes hits; `UTHHealthComponent` with `bCanBeDowned = false`
- Direction (GDD 7.1): few enemies, fast and hard-hitting; tension from stalking and subtle audio rather than numbers.
- ⬜ AI perception (sight, hearing from gunshots / footsteps / doors **and voice**), patrol, investigate, chase, attack. Voice (developer, 2026-11-09): talking in proximity voice is heard by enemies near the speaker with the same weight as footsteps; talking over the **walkie-talkie** carries more weight (it is louder and travels further), so long-range radio calls are a risk.
- ⬜ Enemies can finish off downed players (developer, 2026-11-09)
- ⬜ Stalking: approach from behind / out of view, with quiet, positional audio tells (breathing, a scrape, a footstep) that an attentive player can catch; a short, readable warning before an attack so deaths feel fair
- ⬜ **Difficulty settings** (host's choice, applied by the server): enemy damage first, then possibly enemy speed / health, and helpers for horror players new to tactical shooting that don't remove the skill (e.g. gentler recoil, more reserve ammo). Chosen per mission, shown in the lobby
- ⬜ Hit reactions by bone, death, ragdoll; blood (shared with Milestone 2)
- ⬜ Replication and server authority; spawn / director rules
- ⬜ **Exit:** a server-controlled enemy detects and engages players in a multiplayer test

## Milestone 5: Mission Framework
- ⬜ Mission data assets, objectives, dependencies, extraction
- ⬜ Failure and restart (uses the "everyone down" rule), debrief using `ATHPlayerState` stats
- ⬜ **Found-equipment unlocks** (GDD 12.1): the weapon light, suppressor, extra guns and the flashlight's blacklight mode start locked; finding one in a mission unlocks it for good once the mission is completed. Each player has to pick it up themselves (decided 2026-11-04); saved per player (a SaveGame). The hand flashlight is given right away.
- ⬜ **Loading screen with tips**, written by the developer (e.g. that locked doors can always be opened). Shown during map travel; the tips live in a data asset so they can be added without code.
- ⬜ **Exit:** a test mission progresses, completes, fails and restarts

## Milestone 6: Multiplayer Experience
(Main menu, LAN host and join are done in Milestone 1.)
- ⬜ Lobby: ready-up, player list
- ⬜ **Online with Steam:** OnlineSubsystemSteam, a Steam App ID, invites / join via friends list. Keep the code on the OnlineSubsystem / session interfaces so LAN and Steam are the same path.
- ⬜ **Voice chat** (developer, 2026-11-09): in-game proximity voice (heard by players near you, positioned in the world) and a **walkie-talkie** for long range (push-to-talk, heard by the whole team). Both feed enemy hearing (Milestone 4). Provider to decide with the session provider (Steam / EOS).
- ⬜ Settings menu: graphics, audio (exists), controls / key binding, crosshair (exists), muzzle-flash brightness, blood
- ⬜ The developer's Widget Blueprints for the menus (main menu, settings, lobby, HUD), replacing the placeholder C++ layouts (see AI_CONTENT.md)
- ⬜ **Exit:** host or join over Steam, ready up, start a mission together

## Milestone 7: Loadout Zone (preparation area)
The developer's direction: a physical place you walk around between missions, not just a menu (GDD section 5).
- ⬜ The prep area level: walk-around space shared by the team
- ⬜ Loadout selection: weapons on the walls to pick up (or a menu), primary / secondary from the weapon data assets; equipment (flashlight, night vision, medical, **suppressor**: quieter shots that enemies hear from less far, a smaller muzzle flash, a model on the barrel in first and third person)
- ⬜ Firing range: targets, a grid wall for recoil patterns (an early version comes first, see *Now*). Target carriers with booth buttons exist (2026-11-09); when the range is decorated: a proper button panel instead of the light-switch placeholder, and a sound while a carrier moves.
- ⬜ Mission briefing and debrief area (debrief uses `ATHPlayerState` stats)
- ⬜ Deployment (start the mission when the team is ready) and return to the prep area afterwards
- ⬜ **Exit:** host or join, prepare, deploy, return

## Milestone 8: Story Layout
The developer's story direction is in GDD.md section 9.1 (specialist incident-response team; isolated incidents that turn out to be connected; an infection controlled by a greater host).
- ⬜ Write the story outline in the GDD: the team / organisation, the incident list for the first missions, how the connection is revealed
- ❓ A unique name for the infection / organism and its infected (not "zombie")
- ⬜ Enemy variants from the story: newly infected who still act normal or speak, triggered sleepers, coordinated attacks (feeds Milestone 4's enemy archetypes and the GDD's section 7)
- ⬜ Which mission comes first (the research facility in Milestone 9, or another incident)

## Milestone 9: First Complete Mission
- ⬜ Research facility map, objectives, two enemy types, scripted and dynamic scares, extraction
- ⬜ Four-player test from start to finish
- ⬜ **Exit:** a team completes or fails the mission with everything synchronised

## Later / unscheduled
- ⬜ **Night vision** (developer, 2026-11-09: not sure it stays in the game). The template's goggles and `WB_NightVision` are there but were never tested; its battery texture was renamed `T_NV_Battery`.
- ⬜ **More guns:** shotgun, SMG, DMR / sniper, possibly a second pistol. Data-driven, so mostly content work once the pipeline above is in place. Also attachments (sights, lights, suppressor), if wanted.
- ⬜ Lag compensation for hit registration (known limitation today)
- ⬜ Packaged builds: a Shipping config, a cooked build test, crash reporting; a Steam depot upload when the store page exists
- ⬜ Performance pass: profiling, LODs, decal and casing caps, scalability settings
- ⬜ Accessibility: subtitles for important audio, colour-blind-safe crosshair and markers, flash / gore toggles
- ⬜ Onboarding for horror players new to tactical shooting: the prep-area firing range doubles as a short tutorial (recoil control, reloading, flashlight use)

## Decisions needed (GDD open questions)
- ❓ Friendly fire default (on / off / reduced)
- ❓ Ammo economy: reserve sizes, pickups, resupply between missions
- ❓ How much gore: blood only, or also dismemberment / gibs
- ❓ Crosshair while hip-firing: always, optional, or none (immersion vs readability)
- ❓ Name of the infection / organism (Milestone 8)

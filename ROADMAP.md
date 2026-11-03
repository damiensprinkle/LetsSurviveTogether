# Roadmap

Milestone goals and exit criteria come from GDD.md section 15; this file tracks status and the concrete work under each.
Day-to-day details and "what was just done" live in TASKS.md. Status as of **2026-11-03**.

Legend: ✅ done · 🟡 built, needs play-testing in the editor · 🔧 partly done · ⬜ not started · ❓ needs a design decision first

---

## Now: the next few sessions, in order

1. ✅ **Back up the repository.** Pushed to GitHub `damiensprinkle/LetsSurviveTogether` (`main`) on 2026-11-03, about 5 GB of LFS. Watch the LFS storage and bandwidth on the account's billing page.
2. **Commit the 2026-11-03 work**: third-person hands, lowered / raised carry, pistol retarget, weapon switch, shells, fire-rate fix.
3. **Two-player play test** (Milestone 1 exit). Run *Play As Listen Server* with 2 players and go through the checklist in TASKS.md.
4. **Animation cleanup and naming** (see *Content pipeline*). Do this before adding more guns; it gets harder with every asset that references the old names.
5. **Crosshair and ammo HUD**, then **hit feedback**: hit marker, blood, bullet holes.
6. **Main menu with Host / Join** (Milestone 1 exit, Milestone 6 start).

---

## Milestone 1: Project Foundation
- ✅ Project: FPS_Horror_Template (UE 5.7) with C++ module `TacticalHorror`
- ✅ C++ / Blueprint conventions (TECHNICAL_ARCHITECTURE.md)
- ✅ Git with LFS, backed up to GitHub (`origin/main`; the local branch is `master`, pushed with `git push origin master:main`)
- ✅ Enhanced Input (template context + runtime weapon context)
- ✅ First-person character on `ATHCharacter`; CS-style movement (walk, quiet walk on Shift, crouch)
- 🟡 Listen-server multiplayer (GameMode / GameState / PlayerState, `UTHSessionSubsystem`)
- ⬜ **Exit:** two players connect to a host and move around the same level
- ⬜ Main menu Host / Find / Join (LAN) wired to `UTHSessionSubsystem`

## Milestone 2: Gunplay
**Weapons**
- ✅ Weapon base class, data-driven (`UTHWeaponData`)
- 🟡 M4 (full auto / semi) and Glock 17, first-person arms and animations from the Fab packs
- 🟡 Recoil pattern, view kick and recovery · ADS and hip-fire · reload and ammo · weapon switching
- 🟡 Server-validated firing and damage; fire rate capped for clicking too (fixed 2026-11-03)
- 🟡 Shell casings with collision, bounce sounds, metal material
- 🟡 Muzzle flash (light and quads, player setting for brightness)
- ⬜ Tune the M4 recoil pattern in a test range (grid wall, spray 30, compare)

**Third person (what other players see)**
- 🟡 Body animation in C++: directional walk / crouch, aim pitch, fire / reload / equip actions
- 🟡 Weapon placed from the head; hands IK'd onto the rifle; pistol hands and fingers from its animations
- 🟡 Lowered carry when idle, raised on aim / shot / draw (7 s hold); draw animation with the gun revealed in the hand
- ⬜ Magazine leaves the gun during the third-person reload (hide it while out, or show one in the left hand)
- ⬜ Turn-in-place (feet slide when turning on the spot)
- ⬜ Male rifle walk set (current rifle walks are female `AS_F_*`); pistol crouch; slower crouch walks
- ⬜ Downed / dead body animation (see Milestone 3, Downed)
- ⬜ Small aim correction for the raised pistol (it points where the animation's hand points)

**HUD and feedback**
- ⬜ **Custom crosshairs:** per-weapon style in the weapon data (dot, cross, circle); spread grows with `GetCurrentInaccuracy()`; hidden while aiming down sights; colour / size / opacity in settings
- ⬜ Ammo counter (`GetCurrentAmmo()` / `GetReserveAmmo()`), fire mode, reload prompt
- ⬜ Hit marker (server-confirmed hits only; separate marker and sound for headshots and kills)
- ⬜ **Blood splatter** on hits to players and enemies: a Niagara burst at the hit point, blood decals on nearby walls and floor, and a damage sound. Played on every machine from the existing shot multicast. A player setting turns blood down or off.
- ⬜ **Bullet holes and impact effects by surface:** Physical Materials (concrete, metal, wood, glass, flesh, dirt) on level materials, each picking decal, particles and sound. Candidate decal pack on Fab. Cap decal count and fade them out.
- ⬜ Damage direction indicator, low-health screen effect

**Input**
- ⬜ Real input assets (`IA_Fire`, `IA_Aim`, `IA_Reload`, …) in `IMC_Weapons`, so keys are rebindable from the settings menu

- ⬜ **Exit:** two players use weapons in a test range and see consistent firing and damage

## Content pipeline (between Milestones 2 and 3)
Needed before more guns: there are many animation assets from several packs (Lyra, Sample Animation Pack, UE template, Fab weapon packs) with pack-specific names, and several have been edited.

- ⬜ **Naming and folders.** One folder per weapon and one name pattern, for example:
  - first person: `Content/Weapons/<Gun>/Animations/A_FP_<Gun>_<Action>`, e.g. `A_FP_M4_Reload_Empty`
  - third person: `Content/Animations/ThirdPerson/<Gun>/A_TP_<Gun>_<State>_<Action>`, e.g. `A_TP_M4_Lowered_Walk_F`, `A_TP_G17_Raised_Idle`, `A_TP_M4_HandPose`
  - body sets: `DA_Body_M4`, `DA_Body_M4_Lowered`, `DA_Body_G17`, … (one per gun, instead of per gun type, so each gun can be tweaked)
- ⬜ Copy the animations we use (and have edited) out of the packs into those folders, rename them, fix up redirectors, and point the data assets and scripts at the new names. Do it with a script plus *Fix Up Redirectors*, editor closed.
- ⬜ Delete what's unused: the Fab M4 / G17 copies, the importer leftovers, the unused gun sounds, and possibly whole unused pack folders (Lyra UE4 mannequin animations, MetaHumans if unused). This shrinks the repo and the build.
- ⬜ **New-gun checklist** in ANIMATION_GUIDE.md: import → weapon data → first-person tuning → body sets (raised / lowered) → hand pose → shells → sounds → test.
- ⬜ Decide how `setup_body_anims.py` stays the source of truth versus hand-edited assets (it overwrites values; today the developer's numbers are copied into its tables).

## Milestone 3: Equipment and Atmosphere
- 🔧 Flashlight, night vision, interaction, doors, pickups: template versions work single-player; **replication not verified**. Convert what breaks into replicated components.
- ⬜ **Doors:** server-authoritative open / close, replicated state, sound, can't close on a player; open speed / push direction; locked / keyed variants for missions. Optional third-person hand reach when opening (an additive arm animation).
- 🔧 **Downed and revive.** Built in code, not yet play-tested: at 0 health a player goes down and bleeds out after 30 s; holding **E** for 3 s on them revives at 30% health; when everyone is down the mission restarts. Still to do:
  - ❓ Rules: bleed-out time, whether enemies can finish downed players, whether a downed player can crawl or shoot a pistol, how many downs before death
  - ⬜ Downed body pose and crawl for others to see; the downed first-person view (camera low, desaturated, heartbeat)
  - ⬜ UI: bleed-out timer for the downed player, a revive prompt and progress bar for the reviver, a teammate-down marker
  - ⬜ Revive animation for both players (kneel / help up); cancel on damage or moving away
  - ⬜ Death: spectate a teammate until the mission ends or a respawn point
- 🟡 Health and damage (`UTHHealthComponent`), friendly fire switch on the GameMode
- 🔧 Audio: footsteps by movement mode, gun foley, settings menu volume sliders. ⬜ Footsteps by surface (Physical Materials, shared with bullet impacts), reverb zones, an ambience bed
- ⬜ Environmental lighting pass for a dark test map
- ⬜ **Exit:** players navigate a dark test map with their equipment and interact with objects

## Milestone 4: Enemies and AI
- ⬜ `ATHEnemyBase`: the capsule ignores the Weapon channel and the mesh takes hits; `UTHHealthComponent` with `bCanBeDowned = false`
- ⬜ AI perception (sight, hearing from gunshots / footsteps / doors), patrol, investigate, chase, attack
- ⬜ Hit reactions by bone, death, ragdoll; blood (shared with Milestone 2)
- ⬜ Replication and server authority; spawn / director rules
- ⬜ **Exit:** a server-controlled enemy detects and engages players in a multiplayer test

## Milestone 5: Mission Framework
- ⬜ Mission data assets, objectives, dependencies, extraction
- ⬜ Failure and restart (uses the "everyone down" rule), debrief using `ATHPlayerState` stats
- ⬜ **Exit:** a test mission progresses, completes, fails and restarts

## Milestone 6: Preparation and Multiplayer Experience
- ⬜ Main menu, host LAN, join, lobby (ready-up, player list)
- ⬜ **Online with Steam:** OnlineSubsystemSteam, a Steam App ID, invites / join via friends list. Keep the code on the OnlineSubsystem / session interfaces so LAN and Steam are the same path.
- ⬜ Loadout selection (primary / secondary from the weapon data assets), mission briefing, deployment, return to the preparation area
- ⬜ Settings menu: graphics, audio (exists), controls / key binding, crosshair, muzzle-flash brightness, blood
- ⬜ **Exit:** host or join, prepare, deploy, return

## Milestone 7: First Complete Mission
- ⬜ Research facility map, objectives, two enemy types, scripted and dynamic scares, extraction
- ⬜ Four-player test from start to finish
- ⬜ **Exit:** a team completes or fails the mission with everything synchronised

## Later / unscheduled
- ⬜ **More guns:** shotgun, SMG, DMR / sniper, possibly a second pistol. Data-driven, so mostly content work once the pipeline above is in place. Also attachments (sights, lights, suppressor), if wanted.
- ⬜ Lag compensation for hit registration (known limitation today)
- ⬜ Packaged builds: a Shipping config, a cooked build test, crash reporting; a Steam depot upload when the store page exists
- ⬜ Performance pass: profiling, LODs, decal and casing caps, scalability settings
- ⬜ Accessibility: subtitles for important audio, colour-blind-safe crosshair and markers, flash / gore toggles

## Decisions needed (GDD open questions)
- ❓ Downed rules (see Milestone 3)
- ❓ Friendly fire default (on / off / reduced)
- ❓ Ammo economy: reserve sizes, pickups, resupply between missions
- ❓ How much gore: blood only, or also dismemberment / gibs
- ❓ Crosshair while hip-firing: always, optional, or none (immersion vs readability)

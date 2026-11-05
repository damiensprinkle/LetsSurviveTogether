# Technical Architecture

Engine: **Unreal Engine 5.7.4** · Toolchain: Visual Studio 2026 (MSVC 14.44) · C++ module: `TacticalHorror` (prefix `TH`)

## Where the project came from

| Source | What we took | Notes |
|---|---|---|
| `FPS_Horror_Template 5.7` (Patreon, auto-converted from 5.2) | **Everything** — this is the base. Character, interaction, doors, puzzles, notes, inspect, flashlight, night vision, footsteps, Demo map, MetaHuman Koda | Blueprint-only originally |
| `UnrealFPSHorrorTemplate` (UE 5.7 First Person template, Horror variant) | `Characters/`, `FirstPerson/`, `Input/`, `Variant_Horror/`, missing `LevelPrototyping/` files | Modern first-person arms + anims, flickering horror light, sprint/stamina. It has **no gunplay** |
| UE 5.7 engine template resources | `Content/Weapons/` (rifle, pistol, launcher meshes + fire sound) | Placeholder weapons; `Muzzle` socket |

Neither original project was modified. Backups of the two reparented Blueprints are in `Saved/PreReparentBackup/`.

## Blueprint ↔ C++ split

The template's Blueprints keep doing what they already do well. C++ adds the systems the GDD needs to be robust and replicated.

```
ACharacter
└── ATHCharacter (C++)            health, weapons, downed/revive, speed multipliers
    └── BP_FirstPersonCharacter   camera, movement input, interaction, flashlight, NV, footsteps (template)
        └── BP_Koda               MetaHuman body (the pawn the Demo map spawns)

AGameMode
└── ATHGameMode (C++)             default classes, friendly fire, team wipe -> restart
    └── BP_FirstPersonGameMode    DefaultPawn = BP_Koda (Demo map's override)
```

`ATHCharacter` intentionally creates **no camera, mesh or movement bindings** so it cannot clash with the template.

## Source layout

```
Source/TacticalHorror/
├── Core/          THGameInstance, THGameMode, THGameState (mission phase), THPlayerState (debrief stats), THMenuGameMode (+ menu player controller)
├── Player/        THCharacter, THCharacterMovementComponent, THPlayerController, THBodyAnimInstance, THBodyAnimSet
│   └── Components/THHealthComponent
├── Weapons/       THWeaponBase, THWeaponComponent, THWeaponData, THWeaponTypes, THShellCasing, THAnimNotify_WeaponSound, THHandFlashlight
├── Settings/      THAudioSettingsSubsystem
├── UI/            THAudioSettingsWidget (settings panel), THMainMenuWidget, THSessionEntryWidget, THCrosshairWidget, THAmmoWidget
└── Multiplayer/   THSessionSubsystem
```

Planned (not yet created): `Equipment/`, `AI/`, `Missions/` — see TASKS.md. Interaction stays in the template's Blueprint system (`BPI_Interaction`) unless it needs replication.

## Content layout (2026-11-04 cleanup)

Layout (moves done 2026-11-05):

```
Content/
├── Demo.umap, Maps/          the test map, MainMenu
├── Characters/               body animation sets (BodyAnimSets), UE mannequin animations we use
├── Weapons/                  our guns: Data (DA_*), Blueprints, M4_T0, G17, Ammo, magazines
├── Audio/, Effects/, UI/     ours (footsteps, muzzle flash, crosshairs)
├── Blueprint/                the FPS horror template's Blueprints (BP_FirstPersonCharacter, game mode, widgets);
│                             stays here (scripted moves of it crashed the editor)
├── ThirdParty/<Pack>/        bought / downloaded packs, kept whole: Lyra (from Blueprint/Lyra_Content),
│                             SampleAnimationPack, FPFlashlightAnims, WeaponsFoleySFX, FreeWeaponSFX, TacticalFlashLight
├── Template/                 Interaction (doors, pickups, flashlight pickup), LevelPrototyping (Demo's blocks)
└── _Archive/Animations/      unused animations kept for later, by pack (Lyra, SampleAnimationPack, Mannequins, …)
```

- New packs go in `ThirdParty/<Pack>`: import there, or after adding from Fab move the folder in the editor (drag it, then right-click the folder → Fix Up Redirectors). Fab updates re-download into the pack's original folder.
- Move content only inside the editor, never in Windows Explorer, and pick *Delete unreferenced redirectors* in Fix Up Redirectors. A few assets may fail to save at the old path ("Illegal reference to private object ... ThumbnailInfo"): their moved copy is fine, delete the stale old file once `content_verify.py` shows nothing refers to it. Then update hard-coded paths (`grep -r "/Game/"` in Source, Config, Scripts). Scripted moves (`cleanup_reorganize.py`) work for animations and plain assets but crashed the editor on Blueprints and maps; Python has no redirector fix-up (use the editor's, or `-run=ResavePackages -fixupredirects`).
- C++ refers to content by soft path (loaded at spawn / game start), never with `ConstructorHelpers` on movable content: assets held by a class default can't be moved in the editor.
- `Scripts/Tools/content_inventory.py` reports what the game uses per folder; `content_verify.py` lists leftover redirectors and missing references (the automated tests `TacticalHorror.Content.*` check the same on every run; see TESTING.md) (54 missing editor-preview references came broken with the template and packs: Characters/Heroes, FPSTemplate, ArchVis).
- Deleted in the cleanup (restore from git history if ever needed): MetaHumans, the UE template's Variant_Horror / FirstPerson / Input, the Fab copies of the M4 / G17 packs, the placeholder Rifle / Pistol / grenade launcher, the G17 importer leftovers, and the two packs' showcase maps.

## Systems

### Health — `UTHHealthComponent`
- Server-authoritative. Hooks the owner's `OnTakeAnyDamage`, so `ApplyDamage`/`ApplyPointDamage` from anything works.
- States: `Alive → Downed → Dead`. Downed starts a bleed-out timer (`BleedOutDuration`, default 30 s). `Revive()` restores `ReviveHealthFraction`.
- Damage while downed is **ignored** (placeholder until the GDD decides).
- Friendly fire is asked of `ATHGameMode::IsFriendlyFireEnabled()` (default off).
- When any player stops being Alive the GameMode checks `ATHGameState::AreAllPlayersIncapacitated()`; a team wipe sets phase `Failed` and `ServerTravel("?restart")` after `TeamWipeRestartDelay`.
- Enemies/props: set `bCanBeDowned = false`.

### Revive — `ATHCharacter`
- Hold **E** while looking at a downed teammate within `ReviveRange` for `ReviveDuration` (3 s). Server-timed; cancels if either player moves away or goes down.
- The revive action is created at runtime with `bConsumeInput = false`, so the template's Interact (also E) keeps working.
- `GetReviveProgress()` for a UI bar (not built yet).

### Weapons
- **`UTHWeaponData`** (Data Asset) — all tuning: damage, headshot multiplier, CS-style range falloff, RPM, fire modes, magazine/reserve, reload/equip time, accuracy model, recoil pattern, ADS, move speed, sounds/FX.
- **`ATHWeaponBase`** — hitscan behaviour. Blueprint subclasses (`BP_Weapon_Rifle`, `BP_Weapon_Pistol`) set mesh + data and can implement `On Fired`, `On Reload Started/Finished`, `On Equipped/Unequipped`, `On Fire Mode Changed`, and override `AttachToOwner`.
- **`UTHWeaponComponent`** — loadout spawn (server), switching (client-predicted), ADS (FOV lerp, replicated for movement speed), weapon input.
- **`ATHHandFlashlight`** — a held item that isn't a gun (`IsFirearm()` false: no ammo readout, crosshair or ADS; left mouse switches the light). Given by `UTHWeaponComponent::GiveHandFlashlight` when the owning client sees the template character's `PickupFlashLight` flag turn true (`FlashlightPickupFlag`), added as the next slot (key 3) and inspected. Its own model components: first person in the arms' `hand_r_Flashlight` socket, third person in the body's `hand_r`; `UsesThirdPersonWeaponCopy()` false. One beam per machine, on the copy that machine sees.
- **Weapon light** — `UTHWeaponData` *Weapon Light* section (mesh, attach frame in the muzzle bone's space, lens offset, beam). `ATHWeaponBase` keeps a light model on both copies and one beam on the visible copy. On / unlocked state is replicated on `UTHWeaponComponent` (L, `ToggleWeaponLight`, `UnlockWeaponLight`). `ATHCharacter::PawnClientRestart` removes the template Blueprint's L key binding.

**Gunplay model (CS-inspired)**
- *Recoil:* a fixed, learnable per-shot pattern (`RecoilPattern`, degrees). `ViewKickFraction` of it moves the camera; the rest only moves bullets. Spray index resets after `RecoilResetDelay` without firing. Camera recovers at `ViewRecoveryRate` after firing stops.
- *Accuracy:* cone half-angle from standing/moving (scaled by speed)/air, + per-shot spray growth, × crouch and ADS multipliers. `bFirstShotAccurate` gives a perfect first shot when standing still.
- *Damage:* `Damage × RangeModifierPer10m^(distance/10m) × (HeadshotMultiplier if bone ∈ HeadBoneNames)`.

**Networking**
- Owning client fires instantly (effects, view kick, predicted ammo, predicted impact) → `ServerFire(origin, view rotation, spray index, shot number)` (reliable).
- Server validates: equipped, not reloading, ammo, fire rate (75% tolerance), unique increasing shot number, origin within `MaxOriginError` of the pawn's eyes, spray index within ±2 of its own. It then re-traces using the **same deterministic recoil + seeded spread** (`SpreadSeed` replicated once, hashed with shot number) and applies damage.
- Cosmetics to everyone else via unreliable multicast.
- Not done yet: lag compensation (rewinding targets). Fine for co-op vs AI; revisit if PvP or fast enemies show hit-reg complaints.

**Collision**: custom trace channel **`Weapon`** (`ECC_GameTraceChannel1`, default Block). `ATHCharacter` makes capsules ignore it and meshes block it, so hits report bone names. The horror template's full body (the one other players see) had collision off; `UTHWeaponComponent` turns on queries for the bullet channel only on that body (`bBodyBlocksBullets`, 2026-11-05, found by `Functional.ShotDamage`), and the server keeps its bones updated even when nobody looks at it. Give enemies the same treatment.

**Input**: if `WeaponMappingContext` is empty the component builds one at runtime — LMB fire, RMB aim, R reload, 1/2/3 slots, wheel cycle, B fire mode — at priority 1, above the template's `IMC_Default`. That makes RMB aim instead of the template's camera zoom while a weapon is held. Replace with real `IA_*`/`IMC_*` assets whenever you like (assign them on the component in `BP_FirstPersonCharacter`).

**Shell casings**: `PlayFireEffects` (so on every machine that sees the shot, never the dedicated server) spawns an `ATHShellCasing` at the muzzle + `ShellEjectOffset` (aim space). Not replicated. Its physics body is its own capsule sized from the mesh bounds, so it doesn't rely on the mesh's collision (the casing meshes have none). Physics-only collision (bullets and interaction traces ignore it), passes through pawns, CCD on, a low-bounce / high-friction physical material and strong angular damping so it settles where it lands, one bounce sound on first impact, `ShellLifetime` and `MaxShellCasings` per weapon. All settings are in the *Shell Casing* section of the weapon data.

**What other players see (third person)**
- Each weapon has two copies of its mesh. `Mesh` is the first-person viewmodel (arms included) on the owner's camera, *Only Owner See*. `ThirdPersonMesh` is the same mesh with the arm material slots hidden (`ThirdPersonHiddenMaterialSlots`) and the spare magazine bone hidden (`ThirdPersonHiddenBones`; G17: `Mag1`), holding frame 0 of the idle animation, *Owner No See*. It is attached to the body's `hand_r` and placed every frame after the body has animated.
- Placement: the weapon is placed from the head, not the hand: its `ThirdPersonGripBone` (the rig's own right hand) goes to `ThirdPersonAnchorBone` (`head`) + `ThirdPersonAnchorOffset` in aim space, with the barrel along the body's facing and aim pitch. So the rifle's stock stays in the shoulder and the sights at the eye whatever the source animation's arms do (the rifle animations are female, on a male body). Rifle `(18, 5, -12)`, pistol `(40, 2, -14)`, set by `setup_body_anims.py`. `ThirdPersonRotationOffset` tilts it.
- Hand IK: the weapon gives the body animation (`FTHWeaponGrip`, renewed every frame) the anchor and where the rig's own hands are relative to the grip. The body solves a two-bone IK for each arm onto those points, using the same formula on the worker thread so the hands land on the grips, with the elbow pole forced down and slightly out. With `bThirdPersonMatchRigHands` (default) each hand copies the rig's hand (`FTHHandShape`): a palm frame (X wrist → middle knuckle, Y pinky → index knuckle) and knuckle centre are built from finger bones found by name below each hand bone (`FTHHandShape::FindFingers`, metacarpals skipped) on both skeletons, the body hand bone is placed and turned so its frame matches the rig's, then each finger joint is turned the least amount to point like the rig's. This works across skeletons with different bone axes and hand sizes (the rig's hands are larger, hence matching at the knuckles). `ThirdPersonRightHandOffset` / `ThirdPersonLeftHandOffset` nudge the hands without moving the weapon; `ThirdPersonRight/LeftHandRotation` turn them about the aim axes. Without rig hands, the hands go to the rig's hand bones at the animation's angle. `bThirdPersonLeftHandFollowsRight` keeps the animation's left hand relative to the right instead (unused now). The body set's *Hand Pose* (an animation + frame) replaces the finger bones first; matched rig fingers override it. `th.ThirdPersonTune` adds to all of these live in play, for tuning.
- Lowered carry: a body set's `LoweredSet` is played instead while the weapon isn't ready. `UTHBodyAnimInstance` keeps a `ReadyAlpha` (1 = up) from replicated `IsAiming()` and `RaiseWeapon()` (called by `ATHWeaponBase` on every shot it shows, so every machine agrees), held for `ReadyHoldTime` after the last of them. Idle and walk weights are split between the two sets by `ReadyAlpha` (one shared walk phase), the aim offset and hand IK are scaled by it, and Upper Body Yaw blends between the sets. With the IK off, the weapon is placed so the rig's right palm (knuckle centre and facing, `FTHHandShape`) matches the body's right palm, so it sits in the lowered animation's hand from the first frame.
- During full-pose actions (reload, equip, fire) the IK fades out and the weapon blends onto the right hand at the offset recorded while the hands were on it, so it follows the reload and recoil; afterwards it blends back to the anchor.
- Muzzle flash, shell casings and the shot sound come from whichever copy this machine shows (`GetVisibleMesh`).
- The body is the template's extra `SkeletalMesh` component (ACharacter's own `Mesh` is empty); `UTHWeaponComponent::GetBodyMesh()` finds it. At BeginPlay the weapon component replaces its Anim Blueprint (`ABP_Manny`) with **`UTHBodyAnimInstance`** (`BodyAnimClass`; clear it to get `ABP_Manny` back).
- `UTHBodyAnimInstance` is pure C++ (native pose evaluation, no Anim Blueprint). Legs: idle / four-way walk / crouch blended by velocity direction, speed-matched play rate, shared phase. Upper body (from `spine_01`): the held weapon's aiming pose in mesh-space rotation, so the torso stays on target while walking. Then the pitch aim offset, then actions: fire is additive, reload and equip replace the upper body and are stretched to `ReloadDuration` / `EquipDuration`. It reads only replicated state (velocity, crouch, aim pitch, current weapon, reload), and actions are started where the weapon already plays effects for everyone.
- What to play: `UTHBodyAnimSet` data assets in `Content/Characters/BodyAnimSets`. Any slot a weapon's set leaves empty falls back to `DA_Body_Unarmed` (on the weapon component: Lyra UE5 male walk / crouch). When legs come from the unarmed set, the weapon set's `UpperBodyPose` is layered on the upper body, weighted by that share. `DA_Body_Rifle` uses the SampleAnimationPack's aiming idle and walks (full body), with the unarmed crouch plus layering. `DA_Body_Pistol` uses the unarmed legs plus the UE5 template's `MF_Pistol_Idle_ADS`. Both use Lyra fire / reload / equip. Walk play rate is matched to speed from each clip's root motion (`WalkAnimSpeed` / `CrouchAnimSpeed` override it), within `PlayRateRange`. Built by `Scripts/Tools/setup_body_anims.py`.
- `ATHCharacter::OnStartCrouch/OnEndCrouch` move that body up/down with the capsule (ACharacter only does this for its own `Mesh`).
- Look at it alone: `th.ThirdPerson 1` views your own character from outside, `0` goes back; `th.ThirdPersonView` places the camera (see Scripts/Tools/README.md).

**Movement speed**: `UTHCharacterMovementComponent::GetMaxSpeed()` multiplies whatever the Blueprint sets (walk/sprint/crouch) by the weapon/ADS/downed multiplier.

### Audio settings — `UTHAudioSettingsSubsystem` + `UTHAudioSettingsWidget`
- Game-instance subsystem, `Config=GameUserSettings` (saved per user in `Saved/Config/<platform>/GameUserSettings.ini`). Local only.
- **Master** sets the audio device's transient primary volume (reset to 1 on shutdown so play-in-editor doesn't leave the editor quiet). **Gunshots** multiplies the fire sound volume in `ATHWeaponBase::PlayFireEffects`. To add a category: a property + setter here, a multiplier where those sounds play, a row in the widget. If many categories are needed, switch to Sound Classes + a Sound Mix.
- `ATHPlayerController` toggles the menu with **Escape** or **P** (plain key bindings, not Enhanced Input, so the pawn's contexts can't remove them). Escape never reaches the game in play-in-editor, so use P there. The game keeps running (co-op); input goes UI-only while open. Console: `ToggleAudioSettings`.
- The widget builds its layout in C++. Set `AudioSettingsWidgetClass` on the controller to a Widget Blueprint subclass to restyle it.

### Crosshair — `UTHCrosshairWidget` + `UTHCrosshairSettingsSubsystem`
- `ATHPlayerController` adds `UTHCrosshairWidget` (`CrosshairWidgetClass`) for the local player: a centred image, shown while holding a weapon and looking through your own eyes, hidden while aiming down sights.
- The player's style index, size (8–128 px) and RGB colour live in `UTHCrosshairSettingsSubsystem` (GameUserSettings.ini, saved when the settings panel closes); `OnChanged` updates the crosshair live. The settings panel (`UTHAudioSettingsWidget`, the general settings panel despite its name) has the Crosshair section.
- The style list is project config: `UTHCrosshairWidget::Styles` in `DefaultGame.ini`, pointing at `Content/UI/Crosshairs/T_CH*` (white with a dark outline; the colour tints the white). Add a style = import the PNG (`Scripts/Tools/import_crosshairs.py`) and add a `+Styles=` line.
- The template's `WB_HUD` → `WB_Crosshair` draws a dot (and a hand when looking at something usable). The crosshair widget finds it at run time, keeps its dot hidden and hides itself while the hand shows, so the template's interaction hint still works. Removing the dot from `WB_Crosshair` in the editor would make that step unnecessary.

### Ammo readout — `UTHAmmoWidget`
- Added by `ATHPlayerController` (`AmmoWidgetClass`) next to the crosshair. Bottom right: magazine / reserve, fire mode (only for weapons with more than one mode), "RELOADING" while reloading; the magazine count turns red at or below 25% (`LowAmmoFraction`). Reads `GetCurrentAmmo()` / `GetReserveAmmo()` / `GetFireMode()` / `IsReloading()` from the held weapon each frame and only changes text when a value changes. Hidden without a weapon or in the outside debug view. Widget names for a Blueprint: `MagazineText`, `ReserveText`, `FireModeText`.

### Leave Game — `ATHPlayerController::LeaveGame`
- The settings panel shows **Leave Game** in game only (`bShowLeaveGame`). It calls `UTHSessionSubsystem::LeaveSession()` and opens the start map (`GameDefaultMap`, the main menu). A host leaving closes the server; its clients are sent back to their menu by the engine's network-failure handling. Also a console command (`LeaveGame`).

### Sessions — `UTHSessionSubsystem`
- `HostSession(MapPath, MaxPlayers, bLAN)` → creates session + `ServerTravel(map?listen)`; `FindSessions`, `JoinSession(index)`, `LeaveSession`. Blueprint delegates for each.
- Uses OnlineSubsystem **Null** (LAN) today (`DefaultEngine.ini`). Steam (or EOS) later only touches config + this class.

### Main menu — `ATHMenuGameMode`, `ATHMenuPlayerController`, `UTHMainMenuWidget`, `UTHSessionEntryWidget`
- `/Game/Maps/MainMenu` is the game's start map (`GameDefaultMap`); the editor still opens `Demo`. Its World Settings use `ATHMenuGameMode` (no pawn), whose player controller shows the menu with UI-only input.
- The menu only calls `UTHSessionSubsystem`: **Host Game** → `HostSession(HostMapPath = /Game/Demo, MaxPlayers 4)`; **Find Games** → `FindSessions`, one `UTHSessionEntryWidget` per result, its Join → `JoinSession(index)`; **Settings** opens `UTHAudioSettingsWidget` on top; **Quit**. `HostMapPath` / `MaxPlayers` are config (`DefaultGame.ini`).
- Look: each widget builds a plain layout in code when it has no widget tree. A Widget Blueprint based on the class replaces it without code changes; the C++ finds its widgets by name (`BindWidgetOptional`: `HostButton`, `FindButton`, `SettingsButton`, `QuitButton`, `SessionList`, `StatusText`; rows `JoinButton`, `NameText`, `InfoText`). Point `ATHMenuPlayerController::MainMenuWidgetClass` (config) and the menu's *Session Entry Class* at the Blueprints.
- A client whose host leaves is sent back to the start map (the menu) by the engine's network-failure handling.
- Both maps are in `MapsToCook` (`DefaultGame.ini`), because `Demo` is only reached by name.
- Test helper `th.AutoMenu` (1 = host, 2 = find and join the first game) for off-screen two-instance tests.

### Mission state — `ATHGameState`
- Replicated `ETHMissionPhase` (Preparation, Briefing, Deployment, Active, Extraction, Debrief, Failed) + `OnMissionPhaseChanged`. Mission/objective framework is Milestone 5.

## Conventions
- C++ classes `TH` prefix; Blueprints `BP_`, data assets `DA_`, input `IA_`/`IMC_`, widgets `WBP_`.
- New gameplay systems = C++ component or subsystem + Blueprint/data configuration. No giant classes.
- Server authority for damage, AI, objectives, important interactions. Every replicated feature gets a 2-player PIE test.
- Data Assets for tuning. Don't hardcode weapon/mission numbers in code.
- New C++ content folders go under `Content/` by feature (`Weapons/`, `Missions/`, `AI/`...). The template's folders (`Blueprint/`, `Interaction/`...) stay as they are.

## Building
- Open `LetsSurviveTogether.uproject` → "rebuild modules?" → Yes, or
- `"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LetsSurviveTogetherEditor Win64 Development -Project="<path>\LetsSurviveTogether.uproject" -WaitMutex`
- Re-runnable editor setup script: `Scripts/setup_weapons.py` (see header for the command).

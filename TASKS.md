# Tasks

Small, bounded tasks. Each should end with: compiles, tested in PIE (2 players when networked), committed.

## Now — verify the foundation in the editor
1. **Open the project** (`LetsSurviveTogether.uproject`). Expect a long first shader compile.
2. **Play the Demo map, single player.** Expected: rifle held in front of the camera; LMB fires, RMB aims (FOV narrows), R reloads, 1/2 or wheel switches, B toggles auto/semi.
   - Weapon pointing the wrong way / bad position → edit `CameraHoldOffset` in `Content/Weapons/Data/DA_M4` / `DA_G17`. Loadout is `BP_Weapon_M4` + `BP_Weapon_G17` (the placeholder rifle/pistol stay in the project, unused). Both were imported from the vendor FBX with *Use T0 As Ref Pose* (`SourceArt/M4_FPS_Pack`, `SourceArt/G17_FPS_Pack`); the Fab-installed copies in `Content/Fab` are unused.
   - Reload and draw sounds are anim notifies (**TH Weapon Sound**) on the `WeaponSound` track of the M4 and G17 reload and Draw animations, timed to the magazine/slide/bolt movement. M4 reloads use the Fab *Weapons Foley SFX* pack (`Content/WeaponsFoleySFX`, Rifle_ PullOutTheClip, Insert_The_Clip, Shutter). The G17 uses *FreeWeaponSFX* (`Content/FreeWeaponSFX`, handgun magOut / magIn / slideLock) because the foley pack's pistol clips sounded too heavy. Draw: `sfx_wpn_ar/hgn_foley_aim_01`. On the G17, `mag2_j` is the new magazine (seats at 1.47 s); `mag1_j` is the old one, which is moved off-screen at about 2.0 s, so don't time sounds to it. Each notify starts early by the clip's measured hit time so the hit lands on the motion. All set by `Scripts/Tools/reload_notifies.py`. The old `Audio/Guns/FL_GFX_Gun_Shot_Reloaded_*` imports are unused.
   - G17 slide: the FBX's frame 0 (used as rest pose so the arms fit) has the slide locked back, while the gun was modelled with it there too, so the slide rendered open. The `Slide_j`, `SlideRelease_j` and `Mag1` tracks in all six G17 animations were rewritten to compensate. Re-importing the G17 animations undoes this; redo it (or ask the AI). Open the animation and press play to hear them; click a notify to swap the sound or volume, drag it to retime. `ReloadSound` on the data asset is for guns without animations; leave it empty on these two or it doubles up.
   - Shot sound → `FireSounds` on the data assets (M4: `FL_GFX_Gun_Shot_01` alone, since that pack's shots are different guns and mixing them sounds random; G17: FreeWeaponSFX `sfx_wpn_hgn_gunshot_01–03`, three takes of the same gun. That pack also has separate `*_tail_*` clips, which are not used yet). The previous shot's tail fades over `FireSoundTailFadeOut` so bursts don't pile up. Fast-attack alternatives: 02, 05, 09, 11, 12, 15, 16, 17 (the rest peak 45–90 ms late and feel delayed). Muzzle flash brightness is the player setting `th.MuzzleFlashQuality` (0 low, 1 medium default, 2 high); move it into the settings menu when there is one.
   - Movement is CS-style, no sprint: `StandingWalkSpeed` 260 unarmed on the character's movement component, × `MoveSpeedMultiplier` per weapon (pistol 0.94, rifles 0.88). Shift = walk quietly (× 0.52, footsteps at 30% volume). Crouch is slowest and quietest (115 × weapon, 12% volume; `CrouchSpeed` on the Blueprint and `MaxWalkSpeedCrouched` both 115). The template's sprint nodes in `BP_FirstPersonCharacter` are dead code (Shift is taken by C++) and can be deleted.
3. **Red flashing screen while walking** — fixed. Cause was the Lyra `ABP_Mannequin_Base` on the body (Orientation Warping debug draw in `ABP_ItemAnimLayersBase`); the body now uses the template's `ABP_Manny`. To get Lyra locomotion back, untick *Enable Debug Draw* on the Orientation Warping node and re-test walking.
4. **Two-player test.** Play settings → Number of Players 2, Net Mode *Play As Listen Server*. Check: both players see each other's weapons, shots hurt only with friendly fire on (`BP_FirstPersonGameMode` → *Friendly Fire*), ammo and switching stay in sync.
5. **Downed / revive.** Temporarily enable friendly fire, shoot a teammate down, hold **E** on them for 3 s. Down both players → mission restarts after 5 s.
6. **First commit.** `git add -A && git commit -m "Initial project"` in this folder (LFS is configured for .uasset/.umap).

## Open from the 2026-11-01 session
- **Listen to the audio.** None of it has been heard yet, because automated tests run with `-nosound`. Check footsteps (walk / Shift / crouch), gunshots, and the reload foley on the M4 and G17, both normal and empty reloads.
- Empty reloads may be cut short. `ReloadDuration` comes from the normal reload animation (G17 2.80 s vs empty 3.03 s; M4 3.63 s vs 3.80 s). Fix: add an empty-reload duration to the weapon data.
- Unused content that can be deleted once the developer agrees: the `Content/Fab` copies of the M4 and G17, the G17 importer leftovers (plain materials and `*_ncl1_1` textures in `Weapons/G17`), and `Audio/Guns/FL_GFX_Gun_Shot_Reloaded_*` / `_Safety_*`.
- Possible: a fire-mode switch sound (B) from the foley pack.

## Open from the 2026-11-02 session
- **Audio settings menu** (P in the editor, Escape or P in a packaged game): check that both sliders work by ear, that values survive a restart, and that the mouse cursor and look input come back after closing it. Labels are AI placeholders (AI_CONTENT.md).
- **Shell casings**: check in play that they leave the ejection port, land, clink once, and don't pile up. Tune the *Shell Casing* section of `DA_G17` / `DA_M4` (`ShellEjectOffset`, `ShellEjectVelocity`), or the table in `Scripts/Tools/setup_shells.py`, which overwrites those values when re-run. Also check them in a two-player game. The G17 uses the .45 casing as asked; a 9x19 casing is in `SourceArt/AmmunitionMeshesTextures/Bullet 9x19` if wanted.
- The raw ammo FBX/PNG pack was moved from `Content/Weapons/AmmunitionMeshesTextures` to `SourceArt/AmmunitionMeshesTextures` (sources live outside Content).
- The first git commit hasn't been made yet.
- **Third-person body and weapons** (what other players see): the body now holds the current weapon (aiming pose, walk / crouch legs, aim pitch, fire / reload / equip), and the gun is a copy in its hand instead of floating first-person arms. Checked off-screen with `th.ThirdPersonView`, but not yet in a real two-player session: run task 4 above and look at each other standing, walking in all directions, crouching, looking up and down, firing, reloading and switching weapons. Known rough edges:
  - The left hand isn't locked to the gun; it's where the animation puts it (close on the M4, close on the G17). Hand IK would fix it if it bothers you.
  - Legs are the unarmed walk with the upper body replaced. A male rifle/pistol walk set on the UE5 skeleton (free packs exist) would move better. Drop it into `setup_body_anims.py`'s table.
  - No turn-in-place: feet slide when turning on the spot.
  - Downed / dead have no body animation yet.
  - Tuning: `Third Person` section of `DA_M4` / `DA_G17` (grip offset, rotation) and the `DA_Body_*` sets.
- The template's body used to sink into the floor when crouching (only visible to other players); fixed in `ATHCharacter::OnStartCrouch`.
- Player-vs-player hits: `ATHCharacter` sets the Weapon trace channel on ACharacter's own `Mesh`, which is empty in this template (the body is a separate component). Check friendly-fire hits and headshots in the two-player test.

## Next — Milestone 2 polish
- Third-person weapon handling is done (see above); left-hand IK and a weapon locomotion set are the next steps there.
- Crosshair widget using `GetCurrentInaccuracy()`; ammo HUD using `GetCurrentAmmo()` / `GetReserveAmmo()`.
- Hit marker + impact effects (assign `ImpactEffect` / `ImpactSound` / `MuzzleFlash`).
- Tune `DA_Rifle` recoil pattern in a test range (wall with a grid, spray 30 rounds, compare).
- Real input assets (`IA_Fire`, `IA_Aim`, …, `IMC_Weapons`) so keys are rebindable.

## Later
- Milestone 1 exit: main menu buttons calling `UTHSessionSubsystem` Host / Find / Join (LAN).
- Verify template systems under replication (doors, pickups, flashlight visible to others). Convert what breaks into replicated components.
- Milestone 4: `ATHEnemyBase` (capsule ignores `Weapon` channel, `UTHHealthComponent` with `bCanBeDowned=false`), AI perception, patrol, chase.
- Milestone 5: `MissionManager`, objective data assets, dependencies, extraction, debrief using `ATHPlayerState` stats.
- Decide (GDD open decisions): downed duration & whether enemies can finish downed players, friendly fire, ammo economy.

## Known limitations (by design for now)
- No lag compensation for hit registration.
- ADS speed change uses replicated state, not saved moves — tiny corrections possible under latency.
- Client ammo prediction can be off by one if the server rejects a shot; it resyncs on reload.

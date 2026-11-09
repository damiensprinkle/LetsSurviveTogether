# AI Content Tracking

For Steam's AI content disclosure. Steam asks about AI-generated content players see or hear (art, audio, text, store page). Check the current Steamworks wording before release; it has changed before.

**Rule for this project:** the AI assistant writes code and sets things up. It does not make art, audio, or player-facing writing that ships. Anything AI-made that is in the game is listed under *Replace before release* until a human-made version replaces it.

## Replace before release

AI-made content that is currently in the game. Move a row to *Replaced* when you swap it.

| Asset | What it is | Used by | How to replace |
|---|---|---|---|
| Main menu layout and text: "Host Game", "Find Games", "Settings", "Quit", "Games on your network", "Join", "{0}/{1} players · {2} ms", and the status lines ("Starting a game…", "Searching for games…", "No games found.", "{0} games found.", "Joining…", "Couldn't start the game.", "Couldn't search for games.", "Couldn't join that game.") | Placeholder menu: dark backdrop, grey buttons, the game title in plain text | `UTHMainMenuWidget`, `UTHSessionEntryWidget` (main menu map) | Make a Widget Blueprint based on `THMainMenuWidget` with widgets named `HostButton`, `FindButton`, `SettingsButton`, `QuitButton`, `SessionList`, `StatusText` and set it in `DefaultGame.ini` (`[/Script/TacticalHorror.THMenuPlayerController] MainMenuWidgetClass=...`); the same for the game rows via `THSessionEntryWidget` (`JoinButton`, `NameText`, `InfoText`) and the menu's *Session Entry Class*. The status lines are `LOCTEXT` in `THMainMenuWidget.cpp`; override them by handling *On Status Changed* in the Blueprint, or edit the strings. |
| Ammo readout text and look: the "{0}" / "/ {0}" counts, "RELOADING", bottom-right layout, red when low | Placeholder HUD | `UTHAmmoWidget` (in game) | Widget Blueprint based on `THAmmoWidget` with `MagazineText`, `ReserveText`, `FireModeText` (or handle *On Ammo Changed*), set as the player controller's *Ammo Widget Class*; or edit the `LOCTEXT` strings in `THAmmoWidget.cpp` |
| Settings panel "Leave Game" button | Placeholder label | `UTHAudioSettingsWidget` (in game only) | `LOCTEXT` in `THAudioSettingsWidget.cpp`, or the panel's Widget Blueprint |
| Settings panel crosshair text and look: "Settings", "Crosshair", "Style", "<", ">", "{0} / {1}", "Size", "Red", "Green", "Blue" (and the grey preview square) | Placeholder labels for the crosshair section | `UTHAudioSettingsWidget` (the settings panel) | Edit the `LOCTEXT` strings in `THAudioSettingsWidget.cpp`, or replace the panel with a Widget Blueprint (`AudioSettingsWidgetClass` on the player controller) |
| Audio settings menu text: "Audio", "Master", "Gunplay", "Close" | Placeholder labels and the panel's look (dark box, sliders, percentages) | `UTHAudioSettingsWidget` (Escape / P in game) | Edit the `LOCTEXT` strings in `Source/TacticalHorror/UI/THAudioSettingsWidget.cpp`, or make a Widget Blueprint and set it as `AudioSettingsWidgetClass` on the player controller |

Replacing a texture: save over its source PNG in `SourceArt/`, then in the editor right-click the texture → **Reimport** (Unreal only watches `Content/`, not `SourceArt/`).

## Reference only (not in the game)

AI-made examples to look at while making the real thing. Never import these into `Content/`.

| File | What it is |
|---|---|
| `SourceArt/AI_Reference/REF_MuzzleFlash_Fire.png` | Fiery muzzle flash look: ragged orange petals, deep red-orange centre, lighter tips (CS2-like direction). |

## Made by the AI, technical only

Not art, but listed for completeness. Rebuild them yourself if you want a clean record.

| Asset | What it is |
|---|---|
| `Content/Effects/M_MuzzleFlash` | Unlit additive material: texture × Color × Intensity. |
| `Content/Effects/MI_MuzzleFlash` | Instance of the above used by all guns: Color white (the flash art is already coloured), Intensity 10. Tune the flash here. |
| `Content/Effects/M_BulletHole` | Deferred decal material: texture RGB → base colour, alpha → opacity, roughness 0.9. | No longer shown since 2026-11-09 (the guns use `DA_ImpactSurfaces`); kept as the fallback `ImpactDecal`.
| `Content/Effects/DA_ImpactSurfaces` | Data only, by `setup_impact_decals.py`: which Fab bullet-hole decals each surface gets. No new art. |
| `Content/Effects/PhysicalMaterials/PM_Metal`, `PM_Plaster`, `PM_Paper` | Data only, by `setup_impact_decals.py`: a Physical Material per new surface type. |
| `Content/Characters/BodyAnimSets/DA_Body_*` | Data assets listing which existing animations (Epic's Lyra and UE5 template packs) the third-person body plays per weapon. No new animation. `Scripts/Tools/setup_body_anims.py`. |
| `Content/ThirdParty/OverheadLight/Materials/M_OverheadLight`, `MI_OverheadLight_Case`, `MI_OverheadLight_Tube` | PBR material wired to the Fab light's own textures (colour, normal, roughness, metallic, AO, emissive); the pack's tube emissive map is all black, so the tube's glow (white × tint matching the developer's 5000 K light × strength 60) is AI-set. The mesh was converted from the pack's Collada file (`dae_to_obj.py`), no geometry changed. `Scripts/Tools/import_overhead_light.py`. |
| `Content/ThirdParty/FiringRange` materials, `Content/Maps/L_FiringRange` | The Fab firing range as imported; AI changes: emissive switched off on 17 materials (the FBX wired colour maps to emissive), collision set to complex-as-simple, a player start and 4 placeholder point lights (`TH_PlaceholderLight*`) added. `Scripts/Tools/import_firing_range.py`. |
| `Content/Maps/L_FiringRange`: `TH_RangeShell` folder | Layout only, placed by script from the Fab kit's own meshes and the template's `BP_Door`: ceiling, an empty 16 x 16 m room behind the door with four lights and the player starts, a pinned-exposure PostProcessVolume. No new art. `Scripts/Tools/build_range_room.py`. |
| `Content/Maps/L_EnemyTest` | Layout only, by `build_enemy_test_map.py` from the Fab range kit's meshes and crates, `BP_OverheadLight`, invisible collision boxes for navigation. No new art. |
| `Content/Maps/L_FiringRange`: `TH_RangeTargets` folder | Layout only, by `setup_target_carriers.py`: target carriers and their booth buttons (the template's light switch mesh as a placeholder, "SEND" / "RETURN" text labels). No new art. |
| `Content/Weapons/Ammo/{45,762}/MI_Shell_*`, `Content/Weapons/Ammo/M_Shell_Metal` | Casing materials wired to the vendor's textures: metallic, colour and gloss from the pack's *_SPECULAR, normal map. Capsule collision added to `SM_Shell_*`. `Scripts/Tools/setup_shells.py`, `shell_metal_material.py`. |
| `Content/Animations/ThirdPerson/G17/A_TP_G17_Lowered_Idle` (was `Characters/Mannequins/Anims/Pistol/MM_Pistol_Idle_Hipfire`) | Epic's Lyra UE4 pistol hip-fire idle, retargeted to the UE5 mannequin with Epic's retargeter (no new motion), then hand-posed by the developer. `Scripts/Tools/retarget_pistol_idle.py`. |
| `Content/Weapons/Data/DA_HandFlashlight`, `Content/Characters/BodyAnimSets/DA_Body_Flashlight`, the *Weapon Light* values on `DA_M4` / `DA_G17` | Data assets pointing at the developer's Fab packs (FP Flashlight Animations, FREE Tactical Flashlight) and Epic's Lyra flashlight poses; only the placement numbers and light settings are AI-chosen. `Scripts/Tools/setup_flashlights.py`. |
| `Content/Weapons/M4_T0/SM_M4_Magazine`, `Content/Weapons/G17/SM_G17_Magazine` | The vendor's magazine geometry, split out of each gun's rig unchanged (same triangles and materials) so a dropped / held magazine can be shown. `Scripts/Tools/setup_magazines.py`. |
| `Content/Maps/MainMenu` | Empty level with the menu game mode. `Scripts/Tools/create_main_menu_map.py`. |
| `Content/UI/Crosshairs/T_CH001`–`T_CH008` | The developer's crosshair images (`SourceArt/UI/Crosshairs`, copied from the developer's `CrossHairs+Others` folder), imported as UI textures. Not AI-made; only the import is. `Scripts/Tools/import_crosshairs.py`. |

## Vendor assets edited by the AI (technical, not new art)

| Asset | Change |
|---|---|
| `Content/Weapons/G17/Glock17_Anim_*` (6) | Slide, slide-release and magazine bone tracks corrected for the import's rest pose (`Scripts/Tools/fix_g17_slide.py`). The motion is the vendor's. |
| M4 and G17 reload and Draw animations | **TH Weapon Sound** notifies placed to play clips from the Fab *Weapons Foley SFX* and *FreeWeaponSFX* packs (added by the developer). Only the timing and clip choice are AI-made. |

## Code

C++ under `Source/`, the Python scripts in `Scripts/`, and the project docs were written with an AI coding assistant. Steam's disclosure is about content players experience, not development tools, but note it here in case the wording changes.

## Replaced

| Asset | Replaced with | Date |
|---|---|---|
| `T_MuzzleFlash` (AI star flash) | Developer-made fiery flash, `SourceArt/FX/T_MuzzleFlash.png`. In colour, so `MI_MuzzleFlash` sets **Color** to white. | 2026-10-31 |
| `T_BulletHole` (AI bullet hole) | Developer-made bullet hole, `SourceArt/FX/T_BulletHole.png` | 2026-10-31 |

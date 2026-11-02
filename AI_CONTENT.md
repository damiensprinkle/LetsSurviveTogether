# AI Content Tracking

For Steam's AI content disclosure. Steam asks about AI-generated content players see or hear (art, audio, text, store page). Check the current Steamworks wording before release; it has changed before.

**Rule for this project:** the AI assistant writes code and sets things up. It does not make art, audio, or player-facing writing that ships. Anything AI-made that is in the game is listed under *Replace before release* until a human-made version replaces it.

## Replace before release

AI-made content that is currently in the game. Move a row to *Replaced* when you swap it.

| Asset | What it is | Used by | How to replace |
|---|---|---|---|
| Audio settings menu text: "Audio", "Master", "Gunshots", "Close" | Placeholder labels and the panel's look (dark box, sliders, percentages) | `UTHAudioSettingsWidget` (Escape / P in game) | Edit the `LOCTEXT` strings in `Source/TacticalHorror/UI/THAudioSettingsWidget.cpp`, or make a Widget Blueprint and set it as `AudioSettingsWidgetClass` on the player controller |

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
| `Content/Effects/M_BulletHole` | Deferred decal material: texture RGB → base colour, alpha → opacity, roughness 0.9. |
| `Content/Characters/BodyAnimSets/DA_Body_*` | Data assets listing which existing animations (Epic's Lyra and UE5 template packs) the third-person body plays per weapon. No new animation. `Scripts/Tools/setup_body_anims.py`. |
| `Content/Weapons/Ammo/{45,762}/MI_Shell_*` | Casing material instances (FBX Phong parent) wired to the vendor's diffuse and normal textures. Capsule collision added to `SM_Shell_*`. `Scripts/Tools/setup_shells.py`. |

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

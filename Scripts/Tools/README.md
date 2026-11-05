# Editor tool scripts

One-off editor Python scripts kept for re-use. They were written for a specific job, so read one before running it. Run them headless with the editor closed:

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<abs>\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>\Scripts\Tools\<file>.py" -nullrhi -unattended
```

Output is logged as warnings (`[TH ...]`) in `Saved/Logs/LetsSurviveTogether.log`. An exit code of 1 or 255 with a BINKA decoder ensure in the log is harmless.

The automated tests have their own runner, `Scripts/run_tests.ps1` (see TESTING.md).

| Script | What it does | Notes |
|---|---|---|
| `create_test_map.py` | Creates `/Game/Tests/Maps/L_FunctionalTests` for the play-session tests: a 100 m floor centred on the origin (top at Z = 0), three player starts 2 m apart facing +X, a light. | Re-run to rebuild it. The floor cube's pivot is at a corner, so the script centres it from its bounds and stops if it isn't centred. |
| `import_g17.py` | Imports `SourceArt/G17_FPS_Pack/Glock 17/Glock17.fbx` to `/Game/Weapons/G17` with *Use T0 As Ref Pose*, then saves the folder. | Re-importing replaces the animations. After it, run `fix_g17_slide.py` and `reload_notifies.py` again. |
| `setup_g17.py` | Imports the pack's textures, builds `MI_Glock/Glove/Hand`, sets up `DA_G17`, `BP_Weapon_G17` and the character loadout, then prints the reload timing. | Its muzzle offset is the old estimate; `reload_notifies.py` sets the measured one. |
| `compare_bind_pose.py` | Imports the G17 without T0 to `/Game/Temp_G17Bind` and prints the bones whose rest pose differs from the T0 import. | Delete `/Game/Temp_G17Bind` afterwards. |
| `fix_g17_slide.py` | Rewrites the `Slide_j`, `SlideRelease_j` and `Mag1` tracks in the six G17 animations so the slide sits closed at rest. | **Run it only once, right after a fresh import.** Running it twice applies the correction twice. |
| `measure_muzzle.py` | Reads the G17 mesh's vertices (GeometryScript) and prints the barrel's front face and bore centre in `Main_j` space. | Needs `-EnablePlugins=GeometryScripting` on the command line. |
| `reload_notifies.py` | Rebuilds the `WeaponSound` notify track on the M4 and G17 reload and Draw animations (WeaponsFoleySFX / FreeWeaponSFX clips), and sets the G17 gunshots and muzzle offset. | Safe to re-run. Edit the table at the top to change clips or timing. Our project path has spaces, so copy the script to a path without spaces before passing it to `-script=`. |
| `setup_shells.py` | Imports the .45 and 7.62 casings from `SourceArt/AmmunitionMeshesTextures` to `/Game/Weapons/Ammo`, builds their materials and capsule collision, and fills the *Shell Casing* settings on `DA_G17` / `DA_M4` (offset, velocity, bounce sounds). | Safe to re-run. It overwrites hand-tuned casing values on the data assets, so edit the table at the top instead. |
| `import_crosshairs.py` | Imports `SourceArt/UI/Crosshairs/T_CH*.png` into `/Game/UI/Crosshairs` as UI textures (no mipmaps, never streamed). | Safe to re-run. New styles also need a `+Styles=` line in `DefaultGame.ini` (`THCrosshairWidget`). |
| `create_main_menu_map.py` | Creates `/Game/Maps/MainMenu` with `THMenuGameMode` in its World Settings. | **Not headless** (creating a level needs the full editor): run off-screen like `retarget_pistol_idle.py`; it quits itself. Safe to re-run. |
| `shell_metal_material.py` | Builds `M_Shell_Metal` (metallic; base colour and gloss from the pack's *_SPECULAR texture) and points `MI_Shell_45` / `MI_Shell_762` at it, so casings read as brass / copper instead of dark grey. | Safe to re-run. `setup_shells.py` keeps this material once it exists. Tune with `RoughnessScale` / `Metallic` on the instances. |
| `setup_body_anims.py` | Creates `DA_Body_Unarmed` / `DA_Body_Rifle` / `DA_Body_Pistol` (third-person body animations) in `/Game/Characters/BodyAnimSets`, sets `BodyAnimSet` and `Third Person Anchor Offset` on `DA_M4` / `DA_G17` and the unarmed set on `BP_FirstPersonCharacter`. | Safe to re-run; it overwrites the sets' animation slots and the weapons' third-person hold (anchor, hand offsets / rotations, match rig hands, G17 hidden bones) with its tables. |
| `retarget_pistol_idle.py` | Retargets Lyra's UE4 `MM_Pistol_Idle_Hipfire_UE4` onto the UE5 mannequin as `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Idle_Hipfire` (pistol lowered carry), with Lyra's `RTG_UE4Manny_UE5Manny`. | **Not headless**: the batch retargeter needs Slate. Run in an off-screen editor that quits itself: `UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\pistol_idle_rt.py" -RenderOffScreen -unattended -nosound`. Copy it to a name not starting with `r` first (`\r` in the path is read as an escape). Edit SOURCE/DEST to retarget other UE4 animations. |
| `spread_frame0_edit.py` | Applies the change made to an animation's **first frame** (keyed in the animation editor) to every frame, so a looping animation keeps the edit instead of drifting back. Compares against the original pulled from git. | Edit `EDITED` / `ORIGINAL` at the top. Get the original first: `git show HEAD:<path>.uasset \| git lfs smudge > Content\TempOrig\<same name>.uasset` (from cmd), run the script, then delete `Content\TempOrig`. Back up the edited file first. Used for `AS_F_Rifle_Idle_01` (left upper arm and forearm). |
| `setup_flashlights.py` | Creates `DA_HandFlashlight` (hand flashlight: the FP Flashlight Animations pack's arms idle / draw) and `DA_Body_Flashlight` (Lyra flashlight poses for other players), and fits the Fab tactical light to `DA_M4` / `DA_G17` (*Weapon Light* section: mesh, placement from the muzzle bone, click sound). | Safe to re-run; overwrites the weapon-light placement with its table. Tune live with `th.WeaponLightTune "x y z pitch yaw roll"` (muzzle-bone space), then copy the numbers into the table. |
| `setup_magazines.py` | Cuts the in-gun magazine out of the M4 and G17 rigs as `SM_M4_Magazine` / `SM_G17_Magazine` (triangles skinned to the magazine bone, in that bone's space) and fills *Third Person Reload* on their data assets (magazine bone, mesh, how the left palm holds it, drop / grab / seat timing). | Needs `-EnablePlugins=GeometryScripting`. Safe to re-run; overwrites the timing with its table. |
| `set_m4_spray.py` | Sets `DA_M4`'s Recoil Pattern from measured CS2 M4A4 bullet positions (a curve through them, stored as per-shot steps) and prints all 30 positions. | Edit `MEASURED` to change it. Safe to re-run. |
| `export_foley_wavs.py` | Exports the reload-type foley waves to `Saved/FoleyExport` so `wav_hits.ps1` can read them. | |
| `wav_hits.ps1` | `powershell -File wav_hits.ps1 -Dir <folder>`: prints each WAV's length, peak and hit times in ms (16-bit PCM). | The hit time is how early a notify starts before the motion. |

## Testing without opening the editor

The game can run off-screen and take screenshots:

```
UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" /Game/Demo -game -RenderOffScreen -nosound -ResX=1280 -ResY=720 -windowed "-ExecCmds=th.AutoEquip 1,th.AutoShotInterval 1,th.AutoQuitAfter 6"
```

Screenshots go to `Saved/Screenshots/WindowsEditor/AutoShot_*.png`. The test console variables are in `Source/TacticalHorror/Debug/THAutoTestSubsystem.cpp`: `th.AutoEquip`, `th.AutoCommands` (console commands at set times, e.g. `"3=WeaponLight|6=GiveFlashlight|8=th.ThirdPerson+1"`), `th.AutoFireAfter`, `th.AutoWalk`, `th.AutoShotInterval`, `th.AutoQuitAfter`, `th.PreviewMesh` / `PreviewAnim` / `PreviewSpin` / `PreviewAttach`, `th.HoldOffset` and `th.BlockCameraShakes`. `-nosound` means sound can't be checked this way.

Third person (what other players see), no second player needed:
- `th.ThirdPerson 1` watches your own character from outside; `th.ThirdPerson 0` goes back. Works in the editor console while playing (`~`). It freezes the character's facing while active.
- `th.ThirdPersonView "distance angle height [lookheight]"` moves that camera (default `"200 60 20 40"`: 2 m away, front-right). Angle: 0 front, 90 its right, 180 behind.
- `th.AutoWalk 1/2/3/4` (4 = crouch in place) with `th.AutoWalkAngle` (90 = strafe right, 180 = backwards), `th.AutoLookPitch`, `th.AutoReloadAfter`.
- `th.ThirdPersonTune "anchorX anchorY anchorZ  rightX rightY rightZ  rightPitch rightYaw rightRoll  leftX leftY leftZ  leftPitch leftYaw leftRoll"` adds to the held weapon's third-person hold live (cm / degrees, aim space: X forward, Y right, Z up). Copy the sums into the weapon's data asset or the table in `setup_body_anims.py`. `th.ThirdPersonTune ""` clears it.
- `th.DebugThirdPersonWeapon 1` logs and draws where the third-person weapon copy is placed; 2 also draws it with the default material; 3 shows the hidden arm sections.

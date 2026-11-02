# Editor tool scripts

One-off editor Python scripts kept for re-use. They were written for a specific job, so read one before running it. Run them headless with the editor closed:

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<abs>\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>\Scripts\Tools\<file>.py" -nullrhi -unattended
```

Output is logged as warnings (`[TH ...]`) in `Saved/Logs/LetsSurviveTogether.log`. An exit code of 1 or 255 with a BINKA decoder ensure in the log is harmless.

| Script | What it does | Notes |
|---|---|---|
| `import_g17.py` | Imports `SourceArt/G17_FPS_Pack/Glock 17/Glock17.fbx` to `/Game/Weapons/G17` with *Use T0 As Ref Pose*, then saves the folder. | Re-importing replaces the animations. After it, run `fix_g17_slide.py` and `reload_notifies.py` again. |
| `setup_g17.py` | Imports the pack's textures, builds `MI_Glock/Glove/Hand`, sets up `DA_G17`, `BP_Weapon_G17` and the character loadout, then prints the reload timing. | Its muzzle offset is the old estimate; `reload_notifies.py` sets the measured one. |
| `compare_bind_pose.py` | Imports the G17 without T0 to `/Game/Temp_G17Bind` and prints the bones whose rest pose differs from the T0 import. | Delete `/Game/Temp_G17Bind` afterwards. |
| `fix_g17_slide.py` | Rewrites the `Slide_j`, `SlideRelease_j` and `Mag1` tracks in the six G17 animations so the slide sits closed at rest. | **Run it only once, right after a fresh import.** Running it twice applies the correction twice. |
| `measure_muzzle.py` | Reads the G17 mesh's vertices (GeometryScript) and prints the barrel's front face and bore centre in `Main_j` space. | Needs `-EnablePlugins=GeometryScripting` on the command line. |
| `reload_notifies.py` | Rebuilds the `WeaponSound` notify track on the M4 and G17 reload and Draw animations (WeaponsFoleySFX / FreeWeaponSFX clips), and sets the G17 gunshots and muzzle offset. | Safe to re-run. Edit the table at the top to change clips or timing. Our project path has spaces, so copy the script to a path without spaces before passing it to `-script=`. |
| `setup_shells.py` | Imports the .45 and 7.62 casings from `SourceArt/AmmunitionMeshesTextures` to `/Game/Weapons/Ammo`, builds their materials and capsule collision, and fills the *Shell Casing* settings on `DA_G17` / `DA_M4` (offset, velocity, bounce sounds). | Safe to re-run. It overwrites hand-tuned casing values on the data assets, so edit the table at the top instead. |
| `setup_body_anims.py` | Creates `DA_Body_Unarmed` / `DA_Body_Rifle` / `DA_Body_Pistol` (third-person body animations) in `/Game/Characters/BodyAnimSets`, sets `BodyAnimSet` on `DA_M4` / `DA_G17` and the unarmed set on `BP_FirstPersonCharacter`. | Safe to re-run; it overwrites the sets' animation slots with the table at the top. |
| `export_foley_wavs.py` | Exports the reload-type foley waves to `Saved/FoleyExport` so `wav_hits.ps1` can read them. | |
| `wav_hits.ps1` | `powershell -File wav_hits.ps1 -Dir <folder>`: prints each WAV's length, peak and hit times in ms (16-bit PCM). | The hit time is how early a notify starts before the motion. |

## Testing without opening the editor

The game can run off-screen and take screenshots:

```
UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" /Game/Demo -game -RenderOffScreen -nosound -ResX=1280 -ResY=720 -windowed "-ExecCmds=th.AutoEquip 1,th.AutoShotInterval 1,th.AutoQuitAfter 6"
```

Screenshots go to `Saved/Screenshots/WindowsEditor/AutoShot_*.png`. The test console variables are in `Source/TacticalHorror/Debug/THAutoTestSubsystem.cpp`: `th.AutoEquip`, `th.AutoFireAfter`, `th.AutoWalk`, `th.AutoShotInterval`, `th.AutoQuitAfter`, `th.PreviewMesh` / `PreviewAnim` / `PreviewSpin` / `PreviewAttach`, `th.HoldOffset` and `th.BlockCameraShakes`. `-nosound` means sound can't be checked this way.

Third person (what other players see), no second player needed:
- `th.ThirdPersonView "distance angle height [lookheight]"`, e.g. `"170 90 20 40"`: camera that distance away at that angle around the character (0 front, 90 its right, 180 behind). It freezes the character's facing while active. Also works in the editor console: type `th.ThirdPersonView 170 90 20 40` while playing, and `th.ThirdPersonView ""` to go back.
- `th.AutoWalk 1/2/3/4` (4 = crouch in place) with `th.AutoWalkAngle` (90 = strafe right, 180 = backwards), `th.AutoLookPitch`, `th.AutoReloadAfter`.
- `th.DebugThirdPersonWeapon 1` logs and draws where the third-person weapon copy is placed; 2 also draws it with the default material; 3 shows the hidden arm sections.

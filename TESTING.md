# Automated Tests

Unreal's Automation framework, set up 2026-11-05. The tests catch things that break without anyone noticing (moved
content, a renamed bone, a half-set-up new gun, changed math). How things look and feel still needs play-testing.

## Running them

Close the editor, then from the project folder:

```
.\Scripts\run_tests.ps1                       # all tests (about 200 s, 20 tests)
.\Scripts\run_tests.ps1 -Build                # build first, then test
.\Scripts\run_tests.ps1 TacticalHorror.Data   # only tests whose name starts with this
.\Scripts\run_tests.ps1 "TacticalHorror.Unit+TacticalHorror.Data"   # several groups (+ between them)
```

It prints one line per test and the errors of any that fail; exit code 0 means everything passed. Full report:
`Saved/Automation/Report/index.json`; log: `Saved/Logs/AutomationTests.log`.

In the editor: **Tools → Session Frontend → Automation**, filter `TacticalHorror`, tick and *Start Tests*.

## What is tested

| Test | Checks | Fails when |
|---|---|---|
| `Unit.RecoilPattern` | `GetCumulativeRecoil` sums the pattern steps, repeats the last step, applies `RecoilScale`; fire interval from rpm | the recoil math changes |
| `Data.M4SprayMatchesCS2` | DA_M4's bullets 1, 5, 10 … 30 land on the measured CS2 M4A4 points (±0.01°) | the spray is changed. If on purpose, update the points here and in `set_m4_spray.py` |
| `Data.WeaponSetup` | every weapon Blueprint under `/Game/Weapons`, and the hand flashlight: weapon data set; ammo, rpm, damage, fire mode, fire sounds; first-person mesh and animations on the mesh's skeleton; muzzle, grip, left-hand and magazine bones exist; magazine timings in order; body animation set; weapon light dimmer than the hand flashlight; the flashlight's arms, model, inspect animation and hand socket | a new gun is half set up, a bone or socket is renamed, an animation is on the wrong skeleton, content moved |
| `Data.AnimationNaming` | every animation the weapons, the hand flashlight and the body sets play (and every aim-offset sample) is in our folders under the naming pattern: `Weapons/<Gun>/Animations/A_FP_<Gun>_*`, `Animations/ThirdPerson/<Set>/A_TP_<Set>_*` or `AO_TP_<Set>`; each gun's body set is `DA_Body_<Gun>` | a pack animation is used directly instead of a named copy, or something lands in the wrong folder |
| `Content.NoRedirectors` | no redirectors under `/Game` | a move wasn't followed by *Fix Up Redirectors* |
| `Content.NoMissingReferences` | every asset's references exist, apart from the known broken ones that came with the template and packs (listed in `THContentTests.cpp`) | something in use was deleted or moved outside the editor |
| `Content.CodePathsExist` | content paths written in our C++ (hand flashlight, template pawn) exist | content the code loads by path was moved or renamed |
| `Functional.Weapons` | in a play session on the test map: the player spawns standing on the floor (checked on every play-session test) with the loadout; one click fires one bullet; full auto fires at the weapon's rate; reload refills from the reserve (and does nothing when full); switching to slot 2 draws the pistol and it fires after its draw time; L turns the weapon light and its beam on and off; the hand flashlight goes in slot 3, isn't a firearm, its beam is on when drawn and left mouse turns it off | firing, ammo, reload, switching or the lights break |
| `Functional.FiringRangeSpawn` | a play session in `/Game/Maps/L_FiringRange`: the player spawns in the room behind the range door and is still standing on the floor 2 s later | an imported level loses its collision or player start (the first range import had none: you fell through the floor) |
| `Functional.FiringRangeDoor` | a play session in `/Game/Maps/L_FiringRange`: a ceiling above the player start, a floor and ceiling in the room behind the door; the closed `BP_Door` blocks a player-sized sweep through the doorway, and after the template's `Interact` event the path is clear | the range shell (`build_range_room.py`) loses a piece, or the template door stops opening |
| `Functional.ShotDamage` | a character (no controller, like an enemy) 5 m away, shot once in the chest, loses the weapon's damage | bullets stop reaching bodies (collision, trace channel), damage or range falloff change |
| `Multiplayer.ListenServer` | a host and one client in one session: both spawn on the floor and see each other's weapon; the client's shot is predicted on the client and taken by the server; the client's reload runs on the server and comes back full; the client sees the host reload, the host's weapon light on and off, the host switch to the pistol, and the host's hand flashlight and its beam on and off | replication of firing, ammo, reload, weapons, lights or the flashlight breaks |
| `Multiplayer.DoorSync` | a host and a client in `/Game/Maps/L_FiringRange`, pressing the real interact input: each opens and closes the door and both machines end up the same; the client walks through a door they opened, on the server; a press from out of reach is undone on the client; `BP_Outward_Door` and `BP_SlideDoor` opened by the server before the client knew of them (as a late joiner sees them) | a door stops replicating, or `UTHDoorSyncComponent` is lost from a door Blueprint |
| `Multiplayer.LockedDoors` | a host and a client in `/Game/Maps/L_FiringRange`, with real input. The server spawns the doors and the key in the room behind the range door. `BP_DoorLock`: nobody opens it without a key; the key vanishes for both and only its taker holds it; the other player still can't open it; the holder can, which uses the key up (gone on both machines, and no longer held); then the player with no key closes and opens it. `BP_KeypadDoor`: a code entered on one machine (set as the widget does, the widget itself isn't driven) unlocks it for both. `BP_ValveDoor`: only the wheel answers the interact trace (so only it shows a prompt), bullets still hit the door; holding turns the wheel together on both machines, letting go stops it, the other player can wind it fully open, and a hold at the end turns it back. The client walks through the key door and the valve door, on the server | any of the co-op door components, or the test doors in the range, break |
| `Multiplayer.KeyReturnsWhenHolderLeaves` | a host and two clients in `/Game/Maps/L_FiringRange`: one client holds the key; a third client joining late sees it taken; the holder quits; the key comes back on the server and the remaining clients, and one of them presses it with the real input and holds it | the key's return, or how a taken key reaches clients, breaks |
| `Multiplayer.TargetCarrier` | a host and a client in `/Game/Maps/L_FiringRange`, with the real interact key: the client holds a booth's SEND button and the target moves downrange on both machines, letting go stops it at the same place on both; then the host holds RETURN | the carriers or their buttons break, or the two machines disagree on where a target is |
| `Functional.EnemyTestMap` | a play session in `/Game/Maps/L_EnemyTest`: the player starts on the floor in the start room; the saved navigation mesh reaches the hall, its far corner, the side room and the long corridor; and goes round walls (hall to the corridor below it: much longer than the straight line) | the map or its navigation breaks (e.g. after editing it without rebuilding navigation, or changing the navigation voxel size) |
| `Functional.IronSights` | for the M4 and the G17: aiming puts the rear and front sights on the middle of the screen (within 2 mm), and letting go puts the gun back at the hip | sight points or the alignment break, e.g. after re-importing a gun |
| `Tools.ProfileMeshes` | not a check: with `TH_PROFILE_MESHES` set to skeletal mesh paths separated by `;`, writes `Saved/MeshProfiles/<mesh>.txt`: bones, material slots, and each slot's outline in 1 cm slices along each axis (e.g. a gun's top edge, to find its sights). Otherwise passes doing nothing | |
| `Tools.DumpBlueprintGraphs` | not a check: with `TH_DUMP_BLUEPRINTS` set to asset paths separated by `;`, writes their graphs (including interface functions and timeline curves) as text to `Saved/BlueprintGraphs`. Otherwise passes doing nothing | |

Code: `Source/TacticalHorror/Tests/` (shared play-session helpers in `THTestHelpers.h`). Tests are compiled into development builds only, never into the shipped game.

The play-session tests use `/Game/Tests/Maps/L_FunctionalTests`: a 100 m floor (top at Z = 0), three player starts 2 m apart facing +X, a light. It's made by `Scripts/Tools/create_test_map.py`; re-run that to rebuild it. The tests spawn what else they need. Leave `/Game/Tests` out of packaged builds (Project Settings → Packaging → *Directories to never cook*) when packaging is set up.

## Two PCs (packaged build)

Some things need two real machines: hearing the other player, real network delay, the co-op doors under lag.

1. Close the editor and run `.\Scripts\package.ps1`. The first run cooks everything (a long time); later runs are
   quicker. It makes `Builds\Windows\` and a zip of it, `Builds\LetsSurviveTogether-<date>.zip`.
2. Copy the zip to the other PC (USB stick, shared folder, cloud drive) and extract it anywhere. If the game won't
   start there, run `Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe` from the extracted folder once.
3. Both PCs on the same network. On the host: **Map** to pick `FiringRange` (the default) or `Demo` (the test doors are in the room
   behind the range door), then **Host Game**. On the other: **Find Games**, then **Join**.
4. The first time each PC runs it, Windows Firewall asks whether to allow the game: allow it on **private** networks.
   Without that, Find Games shows nothing.
5. If Find Games still shows nothing (some routers and guest Wi-Fi block it), join by address: on the host, find its
   IPv4 address (`ipconfig` in a command prompt), then on the other PC press the console key (`` ` ``) and type
   `open 192.168.x.x`.

It's a Development build: the console works, and each PC writes a log to `Saved\Logs\LetsSurviveTogether.log` in the
game folder, worth sending back with any bug. Both PCs must run the same build.

## Process

- After each code or content change: build and run the tests before calling it done; a failure is fixed or explained.
- When a bug is fixed, add a test that would have caught it where that's practical.
- Keep the whole run short (target under 2 minutes) so it's always worth running.

## Next steps (ROADMAP.md, Automated testing)

1. ✅ Foundation: runner, unit, data and content tests.
2. ✅ Functional tests in a test map: fire, full auto, reload, switching, both lights, shot damage.
3. ✅ Multiplayer tests: a listen server and a client in one run.
4. Later: a few screenshot comparisons where the picture matters (HUD, crosshair). They need a real GPU (the runner uses none) and break on small lighting or driver changes, so only once the HUD's look is settled.. As features land: doors, downed and revive, hit feedback, enemies (one functional test each), and the friendly-fire rule in the multiplayer test.

## Adding a test

Copy a block from `THWeaponTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHMyTest, "TacticalHorror.Data.MyCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHMyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("what must be true"), Condition);
	return true;
}
```

Name groups: `Unit` (pure math or logic, no assets), `Data` (our data assets), `Content` (the whole content folder),
`Functional` (a play session, one player) and `Multiplayer` (a listen server and a client). For the last two, copy a test
from `THFunctionalTests.cpp` or `THMultiplayerTests.cpp`: steps run in order with `Step(...)`, waits with `FWaitUntil` (a
condition and a timeout) or `FWaitGameTime`, and the test ends with `EndPlay(this)`.

## Bugs the tests found (2026-11-05)

- The body other players see had collision off, so bullets (which pass through the capsule) could never hit another
  player's bones. Fixed: `UTHWeaponComponent` makes the body block the bullet channel only (`bBodyBlocksBullets`).
- A crash in the third-person body animation when the hand IK was still easing off a weapon whose animation set was gone
  (seen as the client watching the host switch to the flashlight). Fixed with a null check in `UTHBodyAnimInstance`.
- Archived template animations refer to editing sequences that never existed (harmless; on the known list).

# Automated Tests

Unreal's Automation framework, set up 2026-11-05. The tests catch things that break without anyone noticing (moved
content, a renamed bone, a half-set-up new gun, changed math). How things look and feel still needs play-testing.

## Running them

Close the editor, then from the project folder:

```
.\Scripts\run_tests.ps1                       # all tests (about 40 s)
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
| `Content.NoRedirectors` | no redirectors under `/Game` | a move wasn't followed by *Fix Up Redirectors* |
| `Content.NoMissingReferences` | every asset's references exist, apart from the known broken ones that came with the template and packs (listed in `THContentTests.cpp`) | something in use was deleted or moved outside the editor |
| `Content.CodePathsExist` | content paths written in our C++ (hand flashlight, template pawn) exist | content the code loads by path was moved or renamed |
| `Functional.Weapons` | in a play session on the test map: the player spawns on the floor with the loadout; one click fires one bullet; full auto fires at the weapon's rate; reload refills from the reserve (and does nothing when full); switching to slot 2 draws the pistol and it fires after its draw time; L turns the weapon light and its beam on and off; the hand flashlight goes in slot 3, isn't a firearm, its beam is on when drawn and left mouse turns it off | firing, ammo, reload, switching or the lights break |
| `Functional.ShotDamage` | a character (no controller, like an enemy) 5 m away, shot once in the chest, loses the weapon's damage | bullets stop reaching bodies (collision, trace channel), damage or range falloff change |
| `Multiplayer.ListenServer` | a host and one client in one session: both spawn on the floor and see each other's weapon; the client's shot is predicted on the client and taken by the server; the client's reload runs on the server and comes back full; the client sees the host reload, the host's weapon light on and off, the host switch to the pistol, and the host's hand flashlight and its beam on and off | replication of firing, ammo, reload, weapons, lights or the flashlight breaks |

Code: `Source/TacticalHorror/Tests/` (shared play-session helpers in `THTestHelpers.h`). Tests are compiled into development builds only, never into the shipped game.

The play-session tests use `/Game/Tests/Maps/L_FunctionalTests`: a 100 m floor (top at Z = 0), three player starts 2 m apart facing +X, a light. It's made by `Scripts/Tools/create_test_map.py`; re-run that to rebuild it. The tests spawn what else they need. Leave `/Game/Tests` out of packaged builds (Project Settings → Packaging → *Directories to never cook*) when packaging is set up.

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

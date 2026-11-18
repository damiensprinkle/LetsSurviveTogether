# Automated Tests

Unreal's Automation framework, set up 2026-11-05. The tests catch things that break without anyone noticing (moved
content, a renamed bone, a half-set-up new gun, changed math). How things look and feel still needs play-testing.

## Running them

Close the editor, then from the project folder:

```
.\Scripts\run_tests.ps1                       # all tests (about 250 s, 28 tests)
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
| `Content.CodePathsExist` | content paths written in our C++ (hand flashlight, template pawn) exist (lists of them too, since 2026-11-11: sounds, blood masks, music) | content the code loads by path was moved or renamed |
| `Content.SeveredPieceMaterials` | every limb piece in `DA_ZombieLooks` / `DA_AmbusherLooks` has a material in each slot (2026-11-15), and none of those material instances has every texture empty (2026-11-16) | a pack's pieces imported without materials (the Priest's were), or with empty ones (the Priest's own set: they fell black): re-run `setup_dismemberment.py` |
| `Content.ArmlessAttack` | `DA_ZombieAnims` has an armless attack with its clip, and the one attack index reaches it after the arm attacks (2026-11-16) | the bite was dropped from the set (`setup_enemies.py anim_sets_only`) or the index mapping broke |
| `Content.SirenBlood` | every look in `DA_MetaHumanLooks` wears the bloodied skin material on both head slots (2026-11-16) | `setup_metahumans.py` rebuilt the looks: re-run `setup_siren_blood.py` |
| `Content.EnemySpatialAudio` | the Windows spatialization plugin is Steam Audio (and loaded, when there is an audio device); `SA_EnemyVoice` and `SA_EnemyFootsteps` are Binaural; every sound in `/Game/Audio/Enemies` (but `LightCrackle`, the player's own) and the charger's stomps are mono, one wave a package (2026-11-16) | the plugin was turned off, an attenuation was rebuilt, or enemy sounds were re-imported in stereo: re-run `make_enemy_sounds_mono.py` |
| `Functional.Weapons` | in a play session on the test map: the player spawns standing on the floor (checked on every play-session test) with the loadout; one click fires one bullet; full auto fires at the weapon's rate; reload refills from the reserve (and does nothing when full), and the magazine it drops is hidden from the holder as it falls and shown once landed; switching to slot 2 draws the pistol and it fires after its draw time; L turns the weapon light and its beam on and off; the hand flashlight goes in slot 3, isn't a firearm, its beam is on when drawn and left mouse turns it off | firing, ammo, reload, switching or the lights break |
| `Functional.FiringRangeSpawn` | a play session in `/Game/Maps/L_FiringRange`: the player spawns in the room behind the range door and is still standing on the floor 2 s later | an imported level loses its collision or player start (the first range import had none: you fell through the floor) |
| `Functional.FiringRangeDoor` | a play session in `/Game/Maps/L_FiringRange`: a ceiling above the player start, a floor and ceiling in the room behind the door; the closed `BP_Door` blocks a player-sized sweep through the doorway, and after the template's `Interact` event the path is clear | the range shell (`build_range_room.py`) loses a piece, or the template door stops opening |
| `Functional.ShotDamage` | a player character (no controller) 5 m away, shot once in the chest, loses the weapon's damage | bullets stop reaching bodies (collision, trace channel), damage or range falloff change |
| `Multiplayer.ListenServer` | a host and one client in one session: both spawn on the floor and see each other's weapon; the client's shot is predicted on the client and taken by the server; the client's reload runs on the server and comes back full; the client sees the host reload, the host's weapon light on and off, the host switch to the pistol, and the host's hand flashlight and its beam on and off | replication of firing, ammo, reload, weapons, lights or the flashlight breaks |
| `Multiplayer.DoorSync` | a host and a client in `/Game/Maps/L_FiringRange`, pressing the real interact input: each opens and closes the door and both machines end up the same; the client walks through a door they opened, on the server; a press from out of reach is undone on the client; `BP_Outward_Door` and `BP_SlideDoor` opened by the server before the client knew of them (as a late joiner sees them) | a door stops replicating, or `UTHDoorSyncComponent` is lost from a door Blueprint |
| `Multiplayer.LockedDoors` | a host and a client in `/Game/Maps/L_FiringRange`, with real input. The server spawns the doors and the key in the room behind the range door. `BP_DoorLock`: nobody opens it without a key; the key vanishes for both and only its taker holds it; the other player still can't open it; the holder can, which uses the key up (gone on both machines, and no longer held); then the player with no key closes and opens it. `BP_KeypadDoor`: a code entered on one machine (set as the widget does, the widget itself isn't driven) unlocks it for both. `BP_ValveDoor`: only the wheel answers the interact trace (so only it shows a prompt), bullets still hit the door; holding turns the wheel together on both machines, letting go stops it, the other player can wind it fully open, and a hold at the end turns it back. The client walks through the key door and the valve door, on the server | any of the co-op door components, or the test doors in the range, break |
| `Multiplayer.KeyReturnsWhenHolderLeaves` | a host and two clients in `/Game/Maps/L_FiringRange`: one client holds the key; a third client joining late sees it taken; the holder quits; the key comes back on the server and the remaining clients, and one of them presses it with the real input and holds it | the key's return, or how a taken key reaches clients, breaks |
| `Multiplayer.TargetCarrier` | a host and a client in `/Game/Maps/L_FiringRange`, with the real interact key: the client holds a booth's SEND button and the target moves downrange on both machines, letting go stops it at the same place on both; then the host holds RETURN | the carriers or their buttons break, or the two machines disagree on where a target is |
| `Functional.EnemyTestMap` | a play session in `/Game/Maps/L_EnemyTest`: the player starts on the floor in the start room; the saved navigation mesh reaches the hall, its far corner, the side room and the long corridor; and goes round walls (hall to the corridor below it: much longer than the straight line) | the map or its navigation breaks (e.g. after editing it without rebuilding navigation, or changing the navigation voxel size) |
| `Functional.EnemyHit` | `BP_Zombie` (no behaviour) spawned 6 m in front of the player on the test map: it stands on the floor with its feet at the bottom of its capsule, its mesh's geometry reaches from the feet to over 150 cm (it has legs), it has a physics asset and full health (200). The test re-aims each frame until the aim trace is on the wanted bone (the idle sways), then fires. The aim trace hits it on Flesh; a chest shot does the gun's body damage and leaves no bullet hole on the body; Flesh has wound effects, splatter and sounds, and both wound effects take the BloodDroplets collision channel (spawned by the test: the shot's own are culled with no rendering); Flesh is marked as blood, and a flesh hit 1 m from a wall splatters it with Blood on in Settings and leaves nothing with it off (2026-11-17); two headshots kill it, it becomes a ragdoll (capsule off) and 2 s later lies on the floor | enemy collision, the Flesh material, blood setup (`DA_ImpactSurfaces`), the droplets' channel, headshots or the ragdoll break, or the zombie's body or Blueprint changes |
| `Functional.EnemyLineup` | `L_EnemyTest`'s line-up of all 21 zombie and creature bodies (the hall's enemies are given no behaviour first): each has a mesh, a physics asset with bodies, plays its animation set through `UTHEnemyAnimInstance`, and has no behaviour; feet on the floor (not the M03 crawlers); the idle moves it, with bone lengths within 10% of its own (an animation from another skeleton can stretch a body); a trace at its chest hits it on Flesh; a killing hit makes it a ragdoll that falls and lies on the floor, its voice stopped (2026-11-17) | a zombie or creature pack update, a skeleton's retargeting settings, `setup_enemies.py`'s line-up, or `ATHEnemyCharacter`'s body setup |
| `Functional.EnemyBehaviors` | in `L_EnemyTest`'s hall (its own enemies removed; the player and each enemy on one clear line): with no behaviour a zombie stays put and idle; the zombie's animations have a crouch and a crouch walk; a hunter with its senses off paces its circle with the calm walk; a still hunter facing away goes for the player when the weapon light shines on it; a siren 7 m away in plain sight keeps lying there (a person's animations), calling, gets up and hits the player who comes within 2 m, and a shot at its chest once it is up hits and hurts it; a charger seeing the player 11 m away roars (a mutant body, 400 health), charges, misses a sidestep without being stunned and is back after the player 2 s later (2026-11-17), and a second charger hits a still player for its charge damage; a dozen zombies with random looks: at least three different bodies, some wearing clothing that follows the body, and every one takes a shot to the chest; a hunter 9 m in front sees the player, chases, and its blow takes its `AttackDamage`, and blood splatters on the player's screen, but no more with Blood off (2026-11-17) (the hunter counts as aggressive and the music's tension has risen) (the hit view modifier is there too); a hunter facing away doesn't notice a still player but hears a shot and investigates; a pack of two ambushers waits head down (neck bowed at least 20° more than standing) with 90 health, ignores a step 7 m away, springs at a shot 4 m away (outside its 3 m sight, inside its 5 m hearing) and sets off the other; a stalker behind a silent player lies still and unaware; the player turns to look at it and it notices them and hides (out of sight or further away, never closer) without attacking, and keeps running when the player looks away mid-run; crouched, and lurks crouched; when the player then turns their back it comes back to stalk them; a stalker 6.5 m behind a player standing still doesn't run off; a stalker behind a player who looks away, woken by a step, strikes (strike delay cut to 1 s for the test), moans right behind them, lands a blow and hides; the hunter shouts as it goes for the player, and the player cries out when hit; a shot hunter's flinch is over 0.65 s after the shot (it moves again at 0.4 s); an ambusher's get-up fades into its moves; the stalker's blow lands only after its whole moan; of three hunters in a row with a 5 m scream, the one that sees the player calls in the second, which doesn't scream or call in the third; a hunter that hears a noise behind a closed door bangs on it, gives the noise up (give-up time cut to 4 s) and bangs no more; an arm shot off falls wearing the body's materials (2026-11-15); a charger with a knee-high crate between it and the player doesn't roar or crash; a stalker in a dead end, the player 8.5 m off watching it, comes for them after its standoff time (cut to 2 s); a cut leg's bones are drawn collapsed into the hip (2026-11-16); the charger moves with the mutant animations (its run held in place), most of its stomps as it charged are a foot landing; a Priest with its legs and an arm off, standing, crawling, dead and shot on the corpse, has no skin drawn over 120 cm from its pelvis (CPU-skinned from its drawn pose; 2026-11-16); the charger's stomps play as a toe lands (no foot still going down 0.1 s after; the last landings logged) and its wall crash spawns its effect when DA_Charger has one (none since 2026-11-17); the severed arm and leg come to rest, asleep, 5 s on (2026-11-17); a charger meeting a wall 20° off parallel crashes and is stunned (2026-11-17) | the AI controller, perception, the behaviour assets (`setup_enemies.py`), navigation in the map, or attacks break |
| `Functional.Sanity` | on the functional test map (no lamps): `DA_Sanity` loads with heartbeats, a fake enemy look, run and shout, and hallucinations that all have sounds; a new player is at 100; solo, in the dark, sanity drains and the player isn't "alone"; under a spawned point light (after the 5 s lamp listing) it comes back; at 30 the heartbeat has played at least twice in 2 s, the aim wanders and the view's edges darken (a vignette in the camera's post process); footsteps that aren't there play on request, and one picked at random is one heard at that sanity; at 0 a fake enemy comes within 6 s, wearing a body, running, in front of the player and facing them, not replicated, and vanishes as it reaches them without harm; another, aimed at and shot, dissolves into the shot effect; back at 100 the aim settles (2026-11-17) | the sanity rules, `DA_Sanity` (`setup_sanity.py`), the lamp light measure, the fake enemy or the local-shot hook break |
| `Multiplayer.EnemyKill` | a host and a client: the server spawns a zombie in front of the client's player; the client re-aims until its view is on the head where the server has it (checked with the server's trace: the server judges the hit, and the client's copy of the idle sway can be a little off, which made this test fail about two runs in three, 2026-11-11), then fires, up to four times (two headshots kill); the server kills it; the client sees it dead, as a ragdoll, with no health | the client's shots stop reaching enemies on the server, or enemy death stops replicating |
| `Multiplayer.EnemyAttack` | Milestone 4's exit: a host and a client in `L_EnemyTest` (host moved out of sight): the server spawns a hunter 4 m in front of the client's player; the server's AI targets that player, runs at them and hits them; the client sees its own health drop and the hunter come within reach ; the client sees the hunter's random look exactly as the server built it (body, materials, clothing) | enemy AI, movement or attacks stop working with a remote player, or their replication breaks |
| `Multiplayer.Downed` | Host and client in `L_EnemyTest` (enemies removed): the client's player taken to 0 health is downed (the host is up), held crouched on both machines, its weapons blocked, and its status screen says so; a stalker brought in beside the downed player doesn't take them as its prey (even when they hurt it) and goes after the host, who is still up; revived, it stands up and its weapons work; downed again, then the host to 0: the last one standing dies instead of being downed, and the client's screen shows the mission failed, and the dead host's body plays a death performance, the same one on both machines | downed / revive / death rules, the forced crouch or the status screen break |
| `Functional.IronSights` | for the M4 and the G17: aiming puts the rear and front sights on the middle of the screen (within 2 mm), and letting go puts the gun back at the hip | sight points or the alignment break, e.g. after re-importing a gun |
| `Functional.LookSensitivity` | look sensitivity 2 turns the view twice as far as 1; aim sensitivity 0.5 halves turning while aimed. Puts the player's own saved values back | the sensitivity settings stop reaching the look input, e.g. if look input is moved off the player controller |
| `Functional.MouseTurn` | 1000 mouse counts sent through the real input path, 8 samples a frame like a 1000 Hz mouse, turn 175° (0.175° per count) slow, fast and while aimed; a sensitivity converted from CS2 sensitivity 2 turns 44°, as in CS2 | an input setting change (mouse axis sensitivity, legacy input scale, mouse smoothing back on) that the converter's `DegreesPerCount` doesn't know about |
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

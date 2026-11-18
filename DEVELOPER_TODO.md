# Developer to-do

Everything waiting on the developer, gathered in one place (2026-11-14) from TASKS.md, ROADMAP.md, ENEMIES.md,
ENEMY_ASSETS_TODO.md and THIRD_PARTY_ASSETS.csv; those files keep the detail. Tick items off here as they are done.
The second half lists the docs that are out of date or will need updating.

---

## 1. Get or make (content only the developer can bring in)

Enemy assets (detail and where each goes: ENEMY_ASSETS_TODO.md):
- [ ] **Siren voice lines with words** (recording in progress): "help", "please", "over here", "I'm hurt", sobbing, a
  name call; a female and a male voice. Goes in `DA_Siren` → Idle Sounds / Alert Sounds.
- [ ] **Siren clothes** (in progress): re-assemble MHC_Advika, MHC_Hannah and Mara1 (Skotukeda) with a MetaHuman
  Creator outfit (output folder `/Game/MetaHumans`), then ask for `setup_metahumans.py` to be re-run.
- [ ] Optional: more zombie bodies with limb pieces (or colour variants for the Priest, Hazmat, Police,
  Construction). (The drag, the sitting siren's wave and get-up: dropped, 2026-11-16; a hit from behind: in.)
- [ ] Optional: **clothed limb pieces for M02 and F02** (jeans, T-shirt, singlet, skirt), or say it's fine: their
  pieces in the pack are bare skin, so a leg in jeans falls bare (2026-11-15, ENEMY_PLAYTESTS.md).
- [x] **A dust impact** for the charger's wall crash: your `NS_ChargerWallDust` (2026-11-17). Still open: check the
  licence of MetaHuman Blood Overlay Lite (THIRD_PARTY_ASSETS.csv: where it came from).
- [ ] Optional (added 2026-11-14, ENEMY_ASSETS_TODO.md): a female zombie voice pack; a sinister laugh for the stalker.
  (The charger's own roar, its dizzy stun, and a bite for an armless zombie: in, 2026-11-16.)

Audio:
- [x] **3D audio (Steam Audio 4.8.1)** heard in play and kept (developer, 2026-11-17). The toggle's label is
  AI-written (AI_CONTENT.md).

Display:
- [ ] **Settings → Display → Blood** (on / off, 2026-11-17): with it off, shooting a zombie shows no spray or wall
  splatter, a limb coming off sprays none, and an enemy's blow leaves no blood on your screen; the sounds, the view jolt
  and the limbs coming off stay (the cut stumps still show their wound texture). Try both. The label is AI-written.

MetaHuman clothes:
- [ ] **`Content/Fab/MetaHuman/ClassicClogs`, `OA_ClassicClogs`, `WI_ClassicClogs`** (113 files) can't be loaded: they
  were saved by a newer Unreal than 5.7 ("Custom version is too new", 2026-11-17 log). That is what crashed the editor
  when it tried to save them. Delete those three, and get outfits whose Fab download is for 5.7.

Other content:
- [ ] **Credits screen text and design** (required, ROADMAP Milestone 2): CC BY 4.0 credits for Dramatic Death
  (Darkania Works), AmbusherShriek and the Charger stomps (freesound authors, THIRD_PARTY_ASSETS.csv), then the other
  packs. The code side can be built once you say what it should look like.
- [ ] **Menu and HUD Widget Blueprints** (AI_CONTENT.md): main menu, settings, lobby, HUD, status screen; the C++
  layouts and labels are placeholders.
- [ ] **Loading-screen tips** (Milestone 5): the text, in a data asset.
- [ ] **A proper button panel** for the firing-range target carriers (placeholder light switch), and a carrier sound.
- [ ] **A magazine-drop sound** for the third-person reload (`MagazineDropSound`, from the foley packs).
- [ ] **The pistol trigger finger** pose in `DA_Body_G17` → Hand Pose (best done by eye in the editor; ANIMATION_GUIDE.md).

Story (Milestone 8 / 9, GDD section 9):
- [ ] How the lab looks; the Siren guard's conversation and attack (cutscene, button mashing); the keypad puzzle's
  4-digit code.
- [ ] The research wing's design (Administration, Utilities, Containment, Research, Classified Area).
- [ ] Documents and recordings: name plates, the elevator audio log, security footage idea; the ending.

## 2. Do in the editor (housekeeping)

- [ ] Delete the stray `Content/DramaticDeath` folder (2 demo meshes from the import; the pack lives in
  `ThirdParty/DramaticDeath`). Still there 2026-11-14.
- [ ] Move the `.mhpkg` downloads out of `Content/Characters/MetaHuman` (about 550 MB; mhc_advika, mhc_hannah, skotukeda).
- [ ] Set MHC_Advika's and MHC_Hannah's output folder to `/Game/MetaHumans` before any re-assembly.
- [ ] Say yes / no to deleting unused content: the G17 importer leftovers in `Weapons/G17`, `Audio/Guns/FL_GFX_Gun_Shot_Reloaded_*` / `_Safety_*`, the dead sprint nodes in
  `BP_FirstPersonCharacter`.
- [ ] Fill in the **VERIFY** cells in THIRD_PARTY_ASSETS.csv from your Fab library (author, licence, link): 32
  rows, mostly the template's sounds, the weapon packs, the audio packs.
- [ ] `AS_F_ADS_Rifle_Idle_01` has a 31.6° left-forearm change of yours that was kept: was it intended?
- [ ] Re-enter your CS2 sensitivity in Settings → Controls → Convert From (mouse smoothing was fixed 2026-11-12), and
  turn Music & ambience back up to about 100% (the built-in volumes were halved 2026-11-13).

## 3. Decide

- [x] The stalker's voice (decided 2026-11-14): it does **not whisper**. It keeps its moan before a strike, and may
  get a sinister laugh if a sound effect turns up (ENEMY_ASSETS_TODO.md).
- [ ] A hunter the director sends in no longer screams as it appears (it counts as called in, and called hunters don't
  scream or call more: the 2026-11-14 fix). Fine, or should a wave announce itself?
- [ ] The siren shot down to half health: does it still drop on its back to play dead, or stay standing / sitting?
- [ ] HunterScream (5.7 s) and AmbusherShriek (6.4 s): too long? (They can be trimmed like the charger stomps.)
- [ ] Friendly fire default; ammo economy; crosshair while hip-firing (ROADMAP *Decisions needed*). Gore is decided in
  practice (dismemberment is in): confirm and it comes off the list.
- [ ] Night vision: keep or drop (never tested).
- [ ] Chargers rarer or commoner; whether the stalker showing itself every 45-90 s is too often; whether 25 m sensing
  through walls is too knowing.
- [ ] Voice chat provider (Steam / EOS) when Milestone 6 starts.

## 4. Check in play

Newest first. Each item's detail is in its TASKS.md entry (same date).

**Sanity, 2026-11-17 (GDD 7.2; TECHNICAL_ARCHITECTURE.md *Sanity*; tuning in `DA_Sanity`)**
- [ ] Play in the dark for a while (it drains about 1 a second; 0.5 with your flashlight on), or type `Sanity 40` /
  `Sanity 10` / `Sanity 0` in the console; `th.SanityDebug 1` shows the value. Listen: the heartbeat (from 60 down),
  the fake footsteps and enemy sounds (from 70 down; `Hallucinate Footsteps`, `Hallucinate StalkerMoan`, ... plays one
  now), the aim wandering (from 50 down), the edges of the view darkening and pulsing with the heartbeat (from 40 down; `DA_Sanity` → Blackout). At 0 a fake zombie runs at you: let it reach you, then shoot one
  (`Hallucinate FakeEnemy` sends one). Is it scary, too often, too loud? Does the drift fight your aim?
- [ ] After the first-play fixes: the fake zombie now runs facing you, and `Hallucinate Footsteps` / `Hallucinate Running` should be clearly heard (nearer, louder, never behind a wall). Still too quiet? `DA_Sanity` → Hallucinations → Footsteps → Volume.
- [ ] With a second player: does staying together keep you sane (no "alone" drain within 15 m, slow recovery within 6 m)?
- [ ] **A smoke effect of your own** for the fake enemy dissolving (`DA_Sanity` → Fake Enemy Shot Effect): it uses your
  charger dust (`NS_ChargerWallDust`) at 1.5x for now. A darker, longer, rising smoke would read better.

**Playtest fixes and mutant animations, 2026-11-16: ENEMY_PLAYTESTS.md, first section**
- [ ] The Priest's limbs no longer black; no spike where a limb was cut; the charger's stomps on its feet and quieter;
  the charger's mutant moves and its dizzy stun on its feet after a wall crash (a miss no longer stuns it, 2026-11-17); an armless
  zombie's bite; a zombie hit from behind; blood on the sirens' faces (open the editor once so the bloodied skin
  material compiles).

**Playtest fixes, 2026-11-15: ENEMY_PLAYTESTS.md, second section**
- [ ] Limbs falling in the body's colours (and the Priest's textured); the charger's quieter stomps; the charger no
  longer charging into crates or round a corner it was baited behind; a stalker you hold in a room coming for you after 6 s.

**Enemy review, 2026-11-14: ENEMY_PLAYTESTS.md**
- [ ] The 14 likely problems found by reading the enemy code (the director's cap, safe high ground, baiting the
  charger, trigger boxes as walls, leg shots, and the rest): each has how to try it and what the fix would be.
- [ ] The three fixes: a stalker turning to the player still up when its prey is downed; a hunter giving up a noise
  behind a door after 20 s; the hunter scream no longer passing from hunter to hunter.
- [ ] Animation smoothness (same file, last section): the two fixes (get-ups fading into the idle; a shot zombie's
  flinch ending as it moves again: too short now?) and ten things to watch (a leg coming off, the siren's power-walk
  chase, the charge's sliding feet, and the rest).

**Enemies, 2026-11-14**
- [ ] Ambushers: Wait Pose (Lying / Standing / As Behaviour) and the `Lying` / `Standing` tags; a Director pack mixing
  both; the pack springing (one shriek, the rest silent); the face-down Stand Up (about 0.9 s).
- [ ] The Priest losing a limb (its pieces fall with M02's physics); Hazmat, Police and Construction on ambushers.
- [ ] Flashlight flicker pace and the crackle; the zombie flinches (Mixamo hits); the legless crawl.
- [ ] Siren standing terrified (waving at a lone player) and sitting dazed; a sitting siren getting up (blend, no clip).
- [ ] Charger stomps and door slam; hunter scream; door bangs; gore sounds for a limb and a head.

**Enemies, 2026-11-13**
- [ ] A hunter or ambusher banging on a closed door (sound, look, how long); a stalker opening a door; a charger
  slamming one in `L_TestFacility`.
- [ ] Outward doors in co-op (same swing on both screens).
- [ ] Siren hits land on the body you see; MetaHuman hair close up; a re-assembled MetaHuman once shaders finish.
- [ ] Staggers (shape held, look); a limping hunter (x0.8); how far enemy voices and steps carry through walls.
- [ ] Stalker: creeping round behind you in the same room, ducking, dashing, striking at openings, picking the straggler.
- [ ] The Director in `L_TestFacility`: calm, waves out of sight, breather after a peak.
- [ ] Textures capped at 2K: still fine up close?

**Earlier, still open**
- [ ] Downed and revive in co-op (hold E 3 s; both down restarts after 5 s).
- [ ] Health regeneration (5 s delay, 5/s) and the screen blood clearing.
- [ ] Music tension (15% faster near a chasing enemy); audio volumes by ear (footsteps, gunshots, reloads, voices).
- [ ] Two players: third-person body (walk, crouch, aim up / down, reload, switch), other players' flashlights,
  friendly-fire hits and headshots, Leave Game as client and host.
- [ ] Bullet holes and paper targets; iron sights; M4 spray against CS2; shell casings landing.
- [ ] Gun through walls (M4, G17, hand flashlight): nothing poking through.
- [ ] Keypad door with the real widget (only one `BP_KeypadDoor` per level until fixed).
- [ ] Death performances (a teammate's), the death fall, the hit jolt.

## 5. Git

- [ ] **Commit** when ready: about 245 changed files are uncommitted (everything since `1daa544`).
- [ ] **Check the GitHub LFS quota** before pushing: the enemy and blood packs are about 4.8 GB.
- [ ] **Push**: `master` is 9 commits ahead of `origin/main` (`git push origin master:main`).

---

## Doc updates needed

### Out of date now (found 2026-11-14)

Brought up to date 2026-11-14: ENEMIES.md (*Not done yet*, the Hunter, Ambusher and Siren tables, the stalker's
voice), TASKS.md's *Enemy improvements* list, ROADMAP.md Milestone 4 and its header, TECHNICAL_ARCHITECTURE.md's old
Siren entry (marked replaced), GDD.md 9.3 (the stalker moans, no whisper).

- **GDD.md section 7, the first archetype table**: "Ambusher: hides in ceilings, vents, or dark corners" is the
  original concept; the *Ambusher, decided* paragraph under it says what was built (packs standing together), but not
  that they can also wait lying down. Add that, or drop the old table row, as you prefer.
- **ROADMAP.md outside Milestone 4** (*Now*, Milestones 2, 3): not re-checked since 2026-11-10.

### When things land

- **Siren voice lines in**: ENEMIES.md Siren table (Idle / Alert Sounds), THIRD_PARTY_ASSETS.csv (your own recording),
  ENEMY_ASSETS_TODO.md tick.
- **Siren clothes in**: ENEMIES.md Siren (no longer "no outfits"), THIRD_PARTY_ASSETS.csv MetaHumans row.
- **Stalker drag built**: ENEMIES.md Stalker, TECHNICAL_ARCHITECTURE.md *Enemies*, GDD downed rules, TASKS.md item 7,
  a new test in TESTING.md.
- **Credits screen built**: ROADMAP Milestone 2 tick, TECHNICAL_ARCHITECTURE.md *Main menu*, remove the "needs the
  credits screen" notes from TASKS.md and THIRD_PARTY_ASSETS.csv.
- **A decision made in section 3**: GDD.md (*Open questions*) and ROADMAP.md *Decisions needed*.
- **Any new pack**: a THIRD_PARTY_ASSETS.csv row (AI_INSTRUCTIONS.md), and the script that wires it in
  (Scripts/Tools/README.md).
- **After play-testing**: tuned numbers go into the data assets and the setup scripts that write them (otherwise a
  re-run puts the old value back), then ENEMIES.md's tables.

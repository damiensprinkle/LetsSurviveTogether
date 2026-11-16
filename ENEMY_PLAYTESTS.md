# Enemy play tests

Things found by reading the enemy code (review of 2026-11-14) that need a play test before anything is changed. None
of these has been seen happening: each is what the code looks like it will do. For each: how to try it, what would
show the problem, where the code is, and what a fix would be. Tick one off when it is tried, and note what you saw.

Useful while testing: `th.EnemyDebug 1` (what each enemy sees and hears), `th.Director.Debug 1` (the director's
intensity, phase and next wave). Maps: `L_EnemyTest` (one room per kind), `L_TestFacility` (doors, the director).

The three bugs from the same review are fixed and have automated tests (TESTING.md: `Functional.EnemyBehaviors`,
`Multiplayer.Downed`); the last section says what to look at for those.

## Playtest of 2026-11-16: fixed, try again

The developer's notes (with two screenshots of a Priest), what was wrong, and what to look at now. Automated checks:
`Functional.EnemyBehaviors` (the cut leg's bones, the charger's steps and animations), `Content.SeveredPieceMaterials`,
`Content.ArmlessAttack`.

- [ ] **"The limbs that fall off are just black."** They were the Priest's: the pack's own materials for its pieces
  (`DismemberedLimbs/MI_ZombieM09_Priest_Dismember*`) have every texture empty. The pieces now wear the body's own skin,
  head, hair and cassock materials (`setup_dismemberment.py`).
  - Try: shoot the Priest's arms, legs and head off; check the cassock's seams line up on the pieces.
- [ ] **"Weird clipping: the leg that remained came to a point."** The engine's bone hiding shrinks a hidden limb onto
  its parent's reference origin, a point about a body's length from the hip, so the cut leg's skin and robe stretched
  into a long spike. The limb's bones are now drawn shrunk into its own joint (hip, shoulder, neck:
  `ATHEnemyCharacter::CollapseLimb`), and its physics bodies removed as before; the blood spray sits on the stump.
  - Try: legs and arms off, standing, crawling and dead; the Priest (robe) and M04 (suit).
  - Problem if: anything still stretches, or a stump looks pinched in a bad way (the wound mask should cover it).
- [ ] **"The charger's footsteps don't line up with its feet; still a bit loud."** Its steps played every stride's
  distance, not when a foot landed, and each stomp rang for 1.5 s, so at a run three or four overlapped. Now enemy
  steps play as a foot lands (`UTHFootstepComponent` → Foot Bones; a shuffle still steps once well past a stride), the
  stomps are cut to 0.5 s (`import_npc_audio.py`), and they play at 0.3 (`DA_MutantLooks` → Footstep Volume).
  - Problem if: steps double up, or go missing at a slow run. Tuning: Foot Lift Height / Foot Plant Height on BP_Zombie's
    footsteps.
- [ ] **"Hard to make out the direction of enemy footsteps on headphones."** Not changed yet: they are positioned
  (`SA_EnemyFootsteps`), but with plain left/right panning, no HRTF, so in front and behind sound alike; the stomp files
  are also stereo, which spreads them. Waiting on the developer: an HRTF spatialiser plugin (DEVELOPER_TODO).
- [ ] **The charger's new animations** (the developer's Mixamo mutant clips) and its stun: after a crash or a miss it
  stays on its feet, dizzy (Mixamo Dizzy Idle), then roars and comes on (it used to fall and crawl).
  - Try: a wall crash, a sidestepped miss; the run at full charge speed (its feet sliding?).
- [ ] **The charger's wall crash dust** (Vefects' Wind VFX dust specks, spawned 0.4 s then drifting out): does it read
  as a crash? Tuning: `DA_Charger` → Wall Crash Effect Time / Scale. It is a wind effect, so if it looks like motes in
  a sunbeam rather than dust knocked loose, a pack with a real impact burst is the next step.
- [ ] **Blood on the sirens' faces** (forehead or over the nose; MetaHuman Blood Overlay). Check the face isn't black or
  checkered (the bloodied skin material compiles the first time the editor opens it), and the blood sits right.
- [ ] **A zombie with both arms off bites** (Mixamo Zombie Neck Bite) and a zombie shot from behind buckles forward
  (Hit Reaction_Behind).

## Playtest of 2026-11-15: fixed, try again

The developer's notes, what was wrong, and what to look at now. Each has an automated test (`Functional.EnemyBehaviors`,
`Content.SeveredPieceMaterials`) except the stomp's loudness.

- [ ] **"Limbs fall off with different textures, like wrong clothes."** Each zombie picks its skin, head and clothes
  colours at random, but the piece that fell kept the colours its mesh came with, whatever the body
  had picked. The Priest's pieces had no materials at all. Now the piece takes the body's material in each slot of the
  same name (`ATHEnemyCharacter::MatchPieceMaterials`), and the Priest's pieces have the pack's own
  (`setup_dismemberment.py`; 2026-11-16: those turned out empty, so the body's own, above).
  - Try: shoot arms and legs off M04 (suits), F02 and the Priest, a few of each.
  - Still expected: **M02 and F02 limbs fall as bare skin.** The pack's pieces for them have no jeans, T-shirt or skirt
    (their clothes are separate meshes), so a jeans leg falls bare. Fixing that needs clothed pieces (DEVELOPER_TODO).
- [ ] **"Charger footstep too loud."** Its stomps now play at 0.45 (`DA_MutantLooks` → Footstep Volume; 0.3 since 2026-11-16). The roar's
  three warning stomps (Charge Tell Sounds) are unchanged.
- [ ] **"He charges into objects sometimes."** Two causes. Its clear-run check looked at waist height only, while the
  crash check looks at the knees too, so a low crate passed the check and then crashed it. And it never
  looked again as the roar ended (the predicted bait, below). Now the check covers knees to chest, and runs again as
  the roar ends: blocked, it comes round instead and roars again no sooner than Roar Retry Delay (3 s).
  - Try: crates and tables between you and it; step behind a pillar while it roars.
  - Problem if: it never charges in a room with a few crates (the check may now be too strict), or crashes anyway.
- [ ] **"Stalker got stuck in a room. I was blocking the doorway and it wouldn't attack me; it just sat there
  crouching."** It only strikes when you look away, and you were watching it. A cornered attack existed but was called
  off as soon as it was seen. Now: held within 12 m (Standoff Distance) with you facing it, and in your view or not
  moving, for Standoff Time (6 s), it comes for you and doesn't turn back when seen. Cornered within 6 m, the same at once.
  - Try: the same doorway; also simply watching a stalker 8-10 m away in an open room.
  - Problem if: it rushes you too readily in open rooms (it should usually get out of sight first), or still sits
    there. Tuning: `DA_Stalker` → Standoff Time.

## Likely problems

- [ ] **The director's cap counts every enemy.** Max Alive (12) counts hand-placed enemies as well as the ones it
  brought in.
  - Try: in `L_TestFacility` place 12 or more enemies anywhere, play, keep calm for two minutes with `th.Director.Debug 1`.
  - Problem if: "brought in" stays 0 while the next-wave timer keeps running out.
  - Code: `ATHDirector::SpawnWave` (`THDirector.cpp`). Fix: count only the enemies the director spawned, or only those
    within some distance of a player.

- [ ] **A wave that can't spawn waits a whole calm again.** With no spawn point out of sight 15-35 m away, the next try
  is 45-75 s later.
  - Try: stand where every spawn point is in view or too near, watch the debug line's timer reach 0.
  - Problem if: the timer resets to about a minute and nothing came.
  - Code: `ATHDirector::Tick` (after `SpawnWave()`). Fix: on a failed wave, try again in about 5 s.

- [ ] **Hunters sent by the director go home.** One that doesn't find you looks around for 4 s, then walks back to its
  spawn point and paces there, still counting against the cap.
  - Try: when a wave comes (a hunter's shout far off), move two rooms away quietly, light off. Follow the hunter with
    `th.EnemyDebug 1` or the free camera.
  - Problem if: it walks back to where it appeared and stays.
  - Code: `ATHEnemyAIController::OnPossess` sets `Home` once; `IdleOrWander` / `Pace` return to it. Fix: a summoned
    enemy makes the place it lost the trail its new home, or the director sends idle ones again.

- [ ] **Standing on something tall is safe.** A blow needs you within 150 cm up or down, but "close enough" is
  measured flat.
  - Try: jump onto the tallest thing you can reach (a stack of crates, a vehicle, a table if it is high enough) with a
    hunter after you.
  - Problem if: it stands underneath facing you and never swings, and you can shoot it freely.
  - Code: `ATHEnemyCharacter::IsInReach` (the `Z > 150` check), `ATHEnemyAIController::Engage`. Fix: out of reach for a
    few seconds, it backs off out of sight (or waits round a corner) instead of standing there.

- [x] **The charger can be baited round a corner.** Seen in play (2026-11-15: "he charges into objects sometimes") and
  fixed: see *Playtest of 2026-11-15* above.

- [ ] **The charger can leave the navigation mesh.** The charge is plain movement, with no check for a ledge or the
  mesh's edge.
  - Try: get it to charge towards a drop, stairs, or a gap in the floor that enemies normally walk round.
  - Problem if: after the stumble it stands still or walks on the spot, somewhere others never go.
  - Code: `ThinkCharger`, the `Charge` state (`AddMovementInput`). Fix: end the charge (a miss) when the floor ahead is
    off the navigation mesh.

- [ ] **Trigger boxes may count as walls.** Every sight, sound, reach and crash check traces by object type, which
  ignores whether a thing actually blocks: an invisible overlap-only box could hide a stalker, muffle a shot, stop a
  blow or crash a charger.
  - Try: put a Trigger Box between you and each kind of enemy (across the charger's hall; between you and a hunter in
    reach; between a shot and a hunter 20 m away).
  - Problem if: the charger crashes on nothing; the hunter won't swing through it; the shot isn't heard.
  - Code: `IsBlocked`, `CountWallsBetween`, `HasClearRun`, the charge's `TraceAhead` (`THEnemyAIController.cpp`);
    `HasClearReach` (`THEnemyCharacter.cpp`); `UTHLightingSubsystem::Measure`; `ATHDirector::PickSpawnPoint`. Fix: one
    shared helper that only counts things that block pawns or sight.

- [ ] **Small props muffle sound like walls.** Each prop on the line between a noise and an enemy cuts how far it
  carries to 35%, whatever its size (up to four).
  - Try: fire from behind a desk or shelf with a few loose things on it at a hunter 10-15 m away with a clear view.
  - Problem if: `th.EnemyDebug 1` shows no "HEARD" where the same shot in the open is heard.
  - Code: `CountWallsBetween`. Fix: ignore things smaller than about a metre, and things that simulate physics.

- [ ] **Leg shots may beat everything else.** Two rifle hits on one leg (Limb Health 45) and a hunter crawls at about
  125 cm/s; killing it takes seven body shots or two headshots.
  - Try: fight three hunters aiming only at legs, then three aiming at the body. Compare bullets and how safe it felt.
  - Problem if: legs are plainly the best target every time.
  - Tuning: `DA_Hunter` → Limb Health; the crawl's speed (`DA_CrawlerAnims`). Maybe a crawler's blows should hurt more,
    or a leg should take more than an arm.

- [ ] **A dead enemy finishes its line.** A voice isn't stopped when the enemy dies.
  - Try: headshot an ambusher as it shrieks (the shriek is 6.4 s), or a hunter mid-scream (5.7 s).
  - Problem if: the corpse keeps screaming.
  - Code: `ATHEnemyCharacter::MulticastVoice_Implementation` (the sound is spawned and forgotten). Fix: keep the audio
    component and fade it out in `ApplyDeath`.

- [ ] **A siren playing dead is a free kill.** The next shot wakes it into a get-up of about 1.7 s. That way up also
  skips its alert cry.
  - Try: shoot a turned siren to half health; when it drops, keep shooting.
  - Problem if: it dies on the floor or halfway up every time, and playing dead never matters.
  - Code: `ATHEnemyAIController::NotifyDamaged` (the play-dead block), `ThinkSiren`. Also on DEVELOPER_TODO (*Decide*):
    whether it plays dead at all.

- [ ] **A stalker near you makes a lone hunter circle.** Flanking counts any enemy with the same target as "another
  one after you", stalkers included.
  - Try: one stalker following you and one hunter, in `L_TestFacility`; let the hunter see you from 10 m.
  - Problem if: the hunter goes to your side first instead of coming straight on.
  - Code: `ATHEnemyAIController::Flank`. Fix: count only enemies that are chasing or attacking.

- [ ] **Hitches when a stalker picks where to go.** Choosing a hiding place can be about 1,600 line traces and 32 path
  searches in one frame; a stalking place somewhat fewer, every 2 s. Enemies also see each other (wasted sight checks).
  - Try: `stat unit` and `stat game` with one stalker, then three, on the host; look at it so it hides.
  - Problem if: a spike in Game time each time it hides or moves on.
  - Code: `FindHidingSpot`, `FindStalkSpot`; the perception setup in the controller's constructor. Fix: spread the
    search over a few frames; have enemies' sight ignore other enemies.

- [ ] **Three of the five siren looks have no clothes** until the MetaHumans are re-assembled with outfits
  (ENEMY_ASSETS_TODO.md).
  - Try: spawn a few sirens (`L_EnemyTest`, re-enter the level a few times).
  - If it matters before the outfits are in: leave MHC_Advika, MHC_Hannah and Mara1 out of `DA_MetaHumanLooks` for now
    (`setup_metahumans.py`).

## The three fixes (2026-11-14): what to look at

Automated tests cover the logic; these are for how it feels.

- [ ] **A stalker and a downed teammate** (two players). Get one player downed with a stalker about. It should turn to
  the player still up within a moment, and come back for the other once they are revived. Before: it froze beside the
  downed player if that player was the host.
- [ ] **A noise behind a door.** Make a noise (a shot) behind a closed door with a hunter or ambusher on the other side
  and stay quiet. It bangs on the door, and gives up about 20 s after the noise (Investigate Give Up Time on its
  behaviour), then goes back to pacing or waiting. A new noise starts it again. Is 20 s right? Locked doors too.
- [ ] **The hunter scream.** With hunters spread through several rooms, let one see you. Only those within 18 m of it
  come; the ones it calls shout (angry) but don't scream and don't call more. A hunter the director sends in no longer
  screams as it appears: it screams when it has lost you and then finds you itself. Does a wave arriving now feel too
  quiet?

## Animation smoothness (second check, 2026-11-14)

Two were fixed and need a look; the rest are to watch. Code: `THEnemyAnimInstance.cpp` unless said.

Fixed, look at how it reads:
- [ ] **Getting up.** The get-up's last frame now fades out under the idle or run over a quarter second (it was dropped,
  and the body jumped in one frame): an ambusher's Stand Up, a charger after a miss, a siren up from playing dead.
- [ ] **A shot zombie's flinch.** A whole-body flinch now ends 0.1 s after the body moves again (the Mixamo clips are
  about 0.75 s, the stagger 0.4 s: it ran on in the flinch, sliding). Is the flinch now too short to read? The other
  way is to hold it still for the clip's length (longer staggers: Stagger Time on `DA_Hunter` / `DA_Stalker`).

To watch:
- [ ] **A leg coming off**: the body goes from upright to prone in one frame (the animation set is swapped with no blend).
- [ ] **A pacing hunter noticing you**: its calm walk swaps to the zombie walk in one frame.
- [ ] **The siren's chase**: at 300 cm/s it plays about 90% walk sped up 1.4x (its run is recorded at 615 cm/s and only
  starts blending in above 280): a power-walk, not a run.
- [ ] **The charge**: 540 cm/s wants the run at 2.8x, capped at 2.5 (Max Run Play Rate): feet slide about 10%, legs frantic.
- [ ] **Walk and run out of step**: the two clips keep separate clocks, so in-between speeds mush (a hunter
  investigating at 80 cm/s is about 17% run).
- [ ] **The pacing hunter at each of its eight points**: a hitch in pace and a 45-degree corner (`Pace`,
  `THEnemyAIController.cpp`).
- [ ] **The stalker at the edge of cover**: its pace may flicker between 180 and 450 cm/s (the exposure check, every 0.2 s,
  has no hold).
- [ ] **Banging on a door**: it snaps to face the door in one frame (`BangOnDoorAhead`).
- [ ] **Turning on the spot** (looking around at a noise, facing you before a blow): the feet slide; needs a
  turn-in-place clip (an asset).
- [ ] **The siren dropping to play dead**: a quarter-second blend from standing to flat on its back; needs a fall clip
  (an asset).

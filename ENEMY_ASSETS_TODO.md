# Enemy assets to get

Things the enemies need that aren't in the project or the packs we have. Each is something the developer gets (a pack,
a download, an export); the code already has a place for it, named below, so it is a drop-in once it is here. No
AI-made content (AI_INSTRUCTIONS.md). Started 2026-11-13; tick items off as they come in.

## Still to get

- [ ] **Siren voice lines with words** (in progress: the developer is recording them) — calls for help ("help",
  "please", "over here", "I'm hurt", sobbing, a name call). A female and a male voice, since the siren's looks are both.
  Now: the voice pack's wordless hurt / scared / sad sounds. Goes in: `DA_Siren` → Idle Sounds (calling), Alert Sounds
  (as it turns).
- [ ] **Siren clothes** (in progress) — re-assemble MHC_Advika, MHC_Hannah and Mara1 (Skotukeda) with an outfit from
  MetaHuman Creator's wardrobe (output folder `/Game/MetaHumans`), then re-run `setup_metahumans.py`. Zombie clothes
  don't fit MetaHumans (different skeleton and proportions); Npc_1 and Npc_2 already have outfits.
- [ ] **A real dust or debris impact effect for the charger's wall crash** (optional) — for now Vefects' Wind VFX dust
  specks as a short puff (below, "In 2026-11-16"); if that doesn't read as a crash in play, a pack with an impact
  burst. Goes in: `DA_Charger` → Wall Crash Effect (and Wall Crash Effect Time 0 for a one-shot).
- [ ] **A female zombie voice pack** — F02 (the Starved female) speaks with the male voices. Goes in: needs a voice
  set per look first (not built), or a female behaviour variant's sounds.
- [ ] **A sinister laugh for the stalker** (optional, developer 2026-11-14: if a sound effect can be found) — it moans
  before a strike and otherwise doesn't speak. Goes in: `DA_Stalker` → Close Sounds (beside the moan).


## More bodies (2026-11-14)

- [x] **More dismemberable zombies** — of the other YI zombies only the Priest (M09) has limb pieces: it joined
  DA_ZombieLooks. Hazmat (M06), Police (M07) and Construction (M08) have none: only ambushers wear them
  (`DA_AmbusherLooks`), and they keep their limbs.
- [ ] **More zombie bodies with limb pieces** (optional) — any further pack with severed pieces (or colour variants for
  the Priest, Hazmat, Police and Construction, which ship one outfit each). Goes in: `setup_enemies.py` → `make_looks`,
  `setup_dismemberment.py` → `SPEC`.

## In (2026-11-16, Mixamo)

- [x] **The charger's own animations** — every Mixamo mutant clip, as `DA_MutantAnims` (`DA_Charger` → Animations):
  Mutant Idle, Mutant Walking (also its calm walk), Mutant Run, Mutant Punch and Mutant Swiping (its attacks), Mutant
  Roaring (its roar). Its stumble after a crash or a miss is now a stun on its feet (developer: "he shouldn't fall
  over he should just be stunned"): it recoils (Zombie Reaction Hit), stands dizzy (Mixamo "Dizzy Idle", the
  developer's find) for its Stumble Down Time, and shakes it off with a short roar. It doesn't flinch when shot
  (developer: it shouldn't). Mutant Breathing Idle is imported, unused. Mutant Walking and Mutant Run came down not
  "In Place": handled (Walk / Run Travel), but "In Place" is the simpler download for later locomotion clips.
- [x] **Wall crash dust** — Vefects' free Wind VFX (`/Game/Vefects/Wind_VFX`): its looping dust specks
  (`NS_Wind_Dust_Specs_01`) spawned at the crash for 0.4 s at 0.6 scale, then drifting out (`DA_Charger` → Wall Crash
  Effect, Effect Time, Effect Scale; `setup_charger_dust.py`). The pack's effect as made, only placed and timed by code.
- [x] **Blood on the sirens' faces** (developer: to fit the environment) — the MetaHuman Blood Overlay pack (Lite,
  `/Game/MHBOP_Lite`): a copy of the MetaHumans' baked skin material with its overlay, and per siren a face material
  with her own settings and one of the pack's two masks (forehead or over the nose), worn through
  `DA_MetaHumanLooks` (`setup_siren_blood.py`; re-run after `setup_metahumans.py`). Faces only: the Lite pack's
  masks are for the head.
- [x] **Zombie hit reaction from behind** — Mixamo "Hit Reaction_Behind": `DA_ZombieAnims` → Hit Reactions, side Back
  (`setup_hit_reactions.py`). Shots from the sides still play any of the three.
- [x] **An attack for a zombie with no arms** — Mixamo "Zombie Neck Bite": `DA_ZombieAnims` → Armless Attacks (new), its
  head's lunge. Its damage is still cut for the arms it lost (36%).

## In (2026-11-14, the developer's downloads and Mixamo exports)

Sounds: `../NpcAudios` (freesound.org, licences in THIRD_PARTY_ASSETS.csv), imported by `import_npc_audio.py`.
Animations: `../Mixamo Animations`, imported and retargeted by `import_mixamo.py`.

- [x] **Charger door slam** — `ChargerDoorSlam`: `DA_Charger` → Door Slam Sounds, at the door as it slams one open.
- [x] **Dismemberment gore** — `DismembermentBody` (an arm or leg) and `DismembermentHead`: `BP_Zombie` → Sever Sounds
  and Head Sever Sounds.
- [x] **Hunter scream** — `HunterScream`: `DA_Hunter` → Scream Sounds, as a chase starts and it calls others in.
- [x] **Stalker light flicker** — `LightCrackle`: `BP_FirstPersonCharacter` → Flicker Sounds, as the light dips. The
  flicker is also slower (steady 0.6-1.6 s between dips; each dip 0.12-0.35 s).
- [x] **Charger footsteps** — `chargerFootsteps.mp3`, four stomps cut into `ChargerStomp_01..04`: `DA_MutantLooks` →
  Footstep Sounds (the charger's own steps).
- [x] **Ambusher burst** — `AmbusherShriek`: `DA_Ambusher` → Alert Sounds; only the ambusher that springs first
  shrieks (the clip is long), the pack it sets off springs silent.
- [x] **Door bangs** — `DoorBang`: hunters and ambushers banging on a closed door.
- [x] **Zombie hit reactions** — Mixamo "Zombie Reaction Hit" and "(1)": `DA_ZombieAnims` → Hit Reactions (the second
  one's start only: it is knocked back after 0.9 s). The siren keeps Lyra's (a person's).
- [x] **A faster zombie crawl** — Mixamo "Zombie Crawl", about 83 cm/s (was 8): `DA_CrawlerAnims` → Walk. It isn't in
  place, so its Walk Travel holds it in place.
- [x] **Zombie getting up fast** — Mixamo "Stand Up", from face down: the ambushers' only playing-dead pose (about
  0.9 s up).
- [x] **The siren waits standing or sitting, never lying** — Mixamo "Terrified" (standing; waves a lone player over
  with Injured Wave Idle) and "Sitting Dazed". Playing dead (shot down) still drops it on its back.

## Dropped

- **A "being dragged" pose and the stalker's drag** (developer, 2026-11-16: maybe a future enhancement) — the stalker's
  side is imported (Mixamo "Pull Heavy Object", `ZM_Pull_Heavy_Object_Anim_mixamo_com`); the drag isn't built.
- **A sitting siren's wave** (developer, 2026-11-16: Mixamo has none) — a sitting siren sits dazed.
- **A siren sitting-to-standing clip** (developer, 2026-11-16: Getting Up is the only one, and it's already in) — a
  sitting siren still gets up by blending into its moves over half a second.
- **Charger floor scrape / pawing** (developer, 2026-11-14) — its roar keeps the three stomps.
- **Stalker lure sounds** (developer, 2026-11-14) — it doesn't speak; it lures with footsteps only (and opens doors).
  Its moan before a strike stays.
- **Blood gush for a severed limb** (developer, 2026-11-14: fine as it is) — the rifle entrance wound effect stays.
- **Stalker whispers** (developer, 2026-11-14) — it doesn't whisper; it makes little moans before attacking (a
  sinister laugh may be added, above).
- **Hunter calls with words** (developer, 2026-11-14) — a hunter doesn't call players with words; its wordless hurt
  and scared sounds while pacing stay.
- **Siren beckon face down** (developer, 2026-11-14) — a siren no longer lies face down.

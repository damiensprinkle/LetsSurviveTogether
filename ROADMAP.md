# Roadmap

Full milestone descriptions and exit criteria: GDD.md section 15. Status as of 2026-10-30.
Legend: ✅ done (compiles; editor-verified where noted) · 🟡 code done, needs in-editor play test · ⬜ not started

## Milestone 1 — Project Foundation
- ✅ Project selected: FPS_Horror_Template (UE 5.7) + C++ module `TacticalHorror`
- ✅ C++/Blueprint conventions (TECHNICAL_ARCHITECTURE.md)
- ✅ Git repository with LFS (first commit 2026-11-02; no remote yet)
- ✅ Enhanced Input (template IMC_Default + runtime weapon context)
- ✅ First-person character (template BP, now on `ATHCharacter`)
- ✅ Basic movement (template)
- 🟡 Listen-server multiplayer (GameMode/State/PlayerState + session subsystem)
- ⬜ Test two players connecting — **exit criterion**

## Milestone 2 — Gunplay
- ✅ Weapon base class · 🟡 Assault rifle · 🟡 Pistol (placeholder meshes)
- 🟡 Recoil (pattern + view kick + recovery) · 🟡 ADS & hip-fire · 🟡 Reloading & ammo · 🟡 Weapon switching
- ⬜ Weapon animations (needs arms setup / Fab weapon with FP anims)
- 🟡 Replicated firing & damage (server-validated)
- ⬜ Two players in a test range — **exit criterion**

## Milestone 3 — Equipment and Atmosphere
- ✅ Flashlight, night vision, interaction, doors, pickups (template — single-player; replication not verified)
- 🟡 Health & damage (`UTHHealthComponent`, downed/revive)
- ⬜ Basic audio system · ⬜ Environmental lighting pass

## Milestones 4–7
Not started: Enemies & AI · Mission framework · Preparation area & sessions UI · First complete mission.

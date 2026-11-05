# Instructions for AI coding agents

Read GDD.md (design), TECHNICAL_ARCHITECTURE.md (how it's built), ROADMAP.md and TASKS.md (what's next) before changing anything.

## Project facts
- UE 5.7.4, C++ module `TacticalHorror` (`Source/TacticalHorror`), class prefix `TH`.
- Build: `"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LetsSurviveTogetherEditor Win64 Development -Project="<abs path>\LetsSurviveTogether.uproject" -WaitMutex` (close the editor first, or use Live Coding).
- Headless editor scripting: `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<file.py> -nullrhi -unattended`. `unreal.log` output only appears in `Saved/Logs/LetsSurviveTogether.log`.
- Re-usable editor scripts, the off-screen test command and the test console variables (`th.*`): `Scripts/Tools/README.md`. Vendor FPS-arms FBX packs need *Use T0 As Ref Pose*; see the G17 notes in TASKS.md.
- Most gameplay content is Blueprint from a paid template (`Content/Blueprint`, `Content/Interaction`). It is binary: you cannot read graphs, only names/paths. Ask the developer when Blueprint behaviour matters.

## Guardrails
- Read existing code before proposing or implementing changes. Follow the established architecture and naming.
- Don't duplicate template systems (interaction, flashlight, night vision, doors, notes, inspect). Extend or wrap them. Exception: footsteps are C++ (`UTHFootstepComponent`) because the template's were driven by Lyra animations the player no longer uses; landing sounds are still the Blueprint's.
- `ATHCharacter` must not add cameras or meshes; the template Blueprint owns those. Movement speed is C++ (`UTHCharacterMovementComponent`: CS-style walk, Shift = walk quietly, no sprint); the Blueprint keeps crouch, jump and look.
- **No AI-made shipped content.** Don't create art, audio or player-facing text that goes into `Content/`; the developer makes those (the game discloses AI content on Steam). Examples for reference go in `SourceArt/AI_Reference/` and are never imported. Log every AI-made asset in `AI_CONTENT.md`.
- Server authority for damage, AI, objectives and important interactions. Think about replication for every feature.
- Tuning goes in Data Assets, not hardcoded values.
- Keep changes focused; one bounded feature per task. Avoid new dependencies/plugins without a reason.
- Don't claim something works without compiling it. Say plainly what still needs an in-editor play test.
- When a third-party package is added (Fab, Epic sample, download, template), add its row to `THIRD_PARTY_ASSETS.csv` in the same change: source, creator, license, link, location, use. Mark unknowns VERIFY rather than guessing.
- After code or content changes, run the automated tests (`Scripts/run_tests.ps1`, editor closed; see TESTING.md) and report the result. When fixing a bug, add a test that would have caught it where practical.
- List any Unreal Editor setup the developer must do.
- Update the docs when architecture or design decisions change. Don't turn GDD suggestions into requirements silently.
- Don't commit unless asked.

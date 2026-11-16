"""
Enemy tactics per behaviour (developer, 2026-11-13: smarter enemies). Sets the Tactics settings on the behaviour data
assets; everything else about them stays as tuned.

- DA_Hunter: flanks (the nearest of several comes on, the others circle to the player's side or back, 4.5 m out),
  zig-zags 1.6 m either side when aimed at, and its shout as a chase starts brings other hunters within 18 m.
- DA_Ambusher: plays dead (lies among the bodies until it springs; setup_play_dead.py), surrounds (3 m out) rather than
  coming single file, and backs off 4 m from a flashlight on it.
- Light and dark: a hunter makes a player out in the dark only within 10 m, a charger 12 m; lit, as far as they see.
- Doors (developer, 2026-11-13): only the stalker opens them, the one in its way (the door's own sound gives it away);
  hunters and ambushers bang on a closed door in their way (door smacks); a charging charger slams one open (in code).
- DA_Charger: three stomps spread over its roar (the warning; Concrete_Land; no floor scrape: developer, 2026-11-14), its own
  door slam sound as it slams one open, and a leg hurt by 150 slows it (x0.75).
  It bursts through unlocked doors and knocks loose props aside (in code).
- DA_Stalker: the lights of a player within 7 m flicker (worse the closer); every 25-50 s, out of sight 8-20 m off, it
  plays a footstep to lure them (no voice; developer, 2026-11-14), and waits there.
- DA_Siren: with more than one player in sight it lies still and quiet; a lone one near it, it calls and beckons (no crawling, 2026-11-13)
  shot down to half health it drops and plays dead, once.

Re-run after changing the values below. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_tactics.py" -nullrhi -unattended
"""
import unreal

BEHAVIORS = '/Game/Enemies/Behaviors/'
# Hunters and ambushers bang on a closed door in their way (developer, 2026-11-13); the developer's DoorBang
# (import_npc_audio.py, 2026-11-14; was the Sound Effects bundle's door smacks)
DOOR_BANGS = ['/Game/Audio/Enemies/DoorBang']
TACTICS = {
    'DA_Hunter': {'flanks': True, 'flank_distance': 450.0, 'weave_offset': 160.0, 'weave_angle': 12.0, 'scream_radius': 1800.0, 'scream_sounds': ['/Game/Audio/Enemies/HunterScream'],
                  'search_ahead': 600.0, 'opens_doors': False, 'bangs_on_doors': True, 'door_bang_sounds': DOOR_BANGS, 'dark_sight_distance': 1000.0},
    # Some lie playing dead, some stand head down (developer, 2026-11-14): 60% lie, unless a placed one chooses (its Wait
    # Pose or a Lying / Standing tag). Its own looks: every zombie plus Hazmat, Police and Construction (setup_enemies.py)
    'DA_Ambusher': {'flanks': True, 'flank_distance': 300.0, 'lies_down': True, 'lies_down_chance': 0.6,
                    'appearance': '/Game/Enemies/Appearance/DA_AmbusherLooks', 'shuns_light': True, 'opens_doors': False,
                    'bangs_on_doors': True, 'door_bang_sounds': DOOR_BANGS},
    # Lights flicker within 7 m; lures with footsteps only (developer, 2026-11-14: it doesn't speak; it used to use the
    # player character's hurt cries too). Its moan before a strike stays.
    'DA_Stalker': {'light_flicker_distance': 700.0, 'lure_sounds': ['/Game/Audio/FootStep/Concrete/CUE/Concrete_Walk'],
                   'lure_interval': unreal.Vector2D(25.0, 50.0), 'opens_doors': True},
    # Works on one player at a time (beckons a lone one; no crawling: developer, 2026-11-13, crawling is for lost legs); plays dead at half health
    'DA_Siren': {'only_for_one': True, 'crawl_animations': None, 'play_dead_health': 0.5, 'opens_doors': False},
    # Stomps as it roars (no floor-scrape sound in the packs); a leg hurt by 150 slows its charge to 75%
    'DA_Charger': {'charge_tell_sounds': ['/Game/Audio/FootStep/Concrete/CUE/Concrete_Land'], 'charge_tell_count': 3,
                   'limb_health': 150.0, 'limp_speed_scale': 0.75, 'dark_sight_distance': 1200.0,
                   'opens_doors': False, 'door_slam_sounds': ['/Game/Audio/Enemies/ChargerDoorSlam']},
}
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH tactics] ' + str(msg))


def player_hurt_sounds():
    """The player character's hurt cries (its Hurt Sounds, soft references), loaded."""
    bp = eal.load_asset('/Game/Blueprint/Character/BP_FirstPersonCharacter')
    cdo = unreal.get_default_object(bp.generated_class())
    sounds = []
    for soft in cdo.get_editor_property('hurt_sounds'):
        asset = unreal.SystemLibrary.load_asset_blocking(soft)
        if asset:
            sounds.append(asset)
    return sounds


def main():
    for name, values in TACTICS.items():
        behavior = eal.load_asset(BEHAVIORS + name)
        for key, value in values.items():
            if isinstance(value, str) and value.startswith('/Game/'):
                value = eal.load_asset(value)
            # Asset paths: the assets
            if isinstance(value, list) and value and isinstance(value[0], str):
                loaded = []
                for v in value:
                    if v == 'PLAYER_HURT':
                        loaded += player_hurt_sounds()
                    else:
                        loaded.append(eal.load_asset(v))
                value = loaded
            behavior.set_editor_property(key, value)
        eal.save_loaded_asset(behavior, False)
        out('%s: %s' % (name, ', '.join('%s %s' % (k, behavior.get_editor_property(k)) for k in values)))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

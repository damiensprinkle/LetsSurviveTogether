"""Creates or updates /Game/Blueprint/Character/DA_Sanity (UTHSanityData; GDD 7.2, developer 2026-11-17).

Sanity drains in the dark and while away from teammates; lit rooms, teammates and calm bring it back. Low sanity:
a heartbeat (HealthDamageSFX's heartbeats), a wandering aim, and sounds that aren't there: the enemies' own sounds
and footsteps (taken from the behaviour assets, so a hallucination sounds exactly like the real thing). At the bottom
a fake zombie rushes the player; shot, it dissolves in the developer's NS_ChargerWallDust (a placeholder until a smoke
effect of its own).

Only the hallucination list, sounds and asset links are set here; the rates and timings keep the C++ defaults on a new
asset and the developer's values on an existing one (TUNE below is applied only when the asset is created).
Headless, editor closed:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nullrhi -unattended
"""
import unreal

ASSET = '/Game/Blueprint/Character/DA_Sanity'
B = '/Game/Enemies/Behaviors/'
VOICE = '/Game/Audio/SA_EnemyVoice'
STEPS = '/Game/Audio/SA_EnemyFootsteps'
HEART = '/Game/ThirdParty/HealthDamageSFX/Cues/Heartbeat/HMNHart_HeartBeat_HA_HealthDmg_%02d_Cue'
GLITCH = '/Game/ThirdParty/HealthDamageSFX/Cues/Damage/Medium/Hits/UIGlitch_MedDmg_HA_HealthDmg_%02d_Cue'


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('Missing ' + path)
    return asset


def behavior_sounds(name, prop):
    return list(load(B + name).get_editor_property(prop))


def entry(name, sounds, below, distance, behind=True, steps=(1, 1), interval=0.5, stride=70.0, attenuation=VOICE, volume=1.0, weight=1.0):
    e = unreal.THHallucinationSound()
    e.set_editor_property('name', name)
    e.set_editor_property('sounds', sounds)
    e.set_editor_property('below_sanity', below)
    e.set_editor_property('weight', weight)
    e.set_editor_property('distance', unreal.Vector2D(*distance))
    e.set_editor_property('behind', behind)
    e.set_editor_property('steps', unreal.IntPoint(*steps))
    e.set_editor_property('step_interval', interval)
    e.set_editor_property('step_length', stride)
    e.set_editor_property('attenuation', load(attenuation))
    e.set_editor_property('volume', volume)
    return e


# name, sounds, heard at or below this sanity, distance (cm), ... The higher the sanity it starts at, the subtler.
HALLUCINATIONS = lambda: [
    entry('Footsteps', [load('/Game/Audio/FootStep/Concrete/CUE/Concrete_Walk')], 70, (350, 700), steps=(4, 8), interval=0.55, stride=70, attenuation=STEPS, volume=2.0, weight=2),
    entry('StalkerBreathing', behavior_sounds('DA_Stalker', 'idle_sounds'), 60, (250, 450)),
    entry('SirenCrying', behavior_sounds('DA_Siren', 'idle_sounds'), 60, (800, 1500), behind=False),
    entry('HunterShout', behavior_sounds('DA_Hunter', 'alert_sounds'), 50, (1200, 2200), behind=False),
    entry('DoorBang', behavior_sounds('DA_Hunter', 'door_bang_sounds'), 45, (700, 1500), behind=False),
    entry('Running', [load('/Game/Audio/FootStep/Concrete/CUE/Concrete_Sprint')], 40, (600, 1100), steps=(5, 9), interval=0.32, stride=120, attenuation=STEPS, volume=1.6),
    entry('ChargerGrowl', behavior_sounds('DA_Charger', 'idle_sounds'), 35, (1000, 1800), behind=False),
    entry('HunterScream', behavior_sounds('DA_Hunter', 'scream_sounds'), 30, (1500, 2500), behind=False, weight=0.5),
    entry('AmbusherShriek', behavior_sounds('DA_Ambusher', 'alert_sounds'), 30, (1500, 2500), behind=False, weight=0.5),
    entry('StalkerMoan', behavior_sounds('DA_Stalker', 'close_sounds'), 25, (120, 220)),
    entry('ChargerStomps', [load('/Game/Audio/Enemies/ChargerStomp_%02d' % i) for i in range(1, 5)], 20, (1200, 1800), behind=False,
          steps=(6, 10), interval=0.4, stride=100, attenuation=STEPS, volume=0.4),
]

TUNE = {}  # e.g. {'darkness_drain': 1.0}: applied only to a new asset

data = unreal.load_asset(ASSET)
created = data is None
if created:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    data = tools.create_asset('DA_Sanity', '/Game/Blueprint/Character', unreal.THSanityData, unreal.DataAssetFactory())
    for key, value in TUNE.items():
        data.set_editor_property(key, value)

data.set_editor_property('heartbeat_sounds', [load(HEART % i) for i in range(1, 12)])
data.set_editor_property('hallucinations', HALLUCINATIONS())
data.set_editor_property('fake_enemy_looks', load('/Game/Enemies/Appearance/DA_ZombieLooks'))
data.set_editor_property('fake_enemy_anims', load('/Game/Enemies/Animations/DA_ZombieAnims'))
data.set_editor_property('fake_enemy_sounds', behavior_sounds('DA_Hunter', 'alert_sounds'))
data.set_editor_property('fake_enemy_attenuation', load(VOICE))
data.set_editor_property('fake_enemy_vanish_sounds', [load(GLITCH % i) for i in range(1, 12)])
data.set_editor_property('fake_enemy_shot_effect', load('/Game/Enemies/Effects/NS_ChargerWallDust'))
if not unreal.EditorAssetLibrary.save_loaded_asset(data, False):
    raise RuntimeError('Could not save ' + ASSET + ' (is the editor open?)')
unreal.log_warning('[TH sanity] %s %s: %d hallucinations, %d heartbeats' % (
    'created' if created else 'updated', ASSET, len(data.get_editor_property('hallucinations')), len(data.get_editor_property('heartbeat_sounds'))))

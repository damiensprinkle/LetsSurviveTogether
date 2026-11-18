"""
Enemies sound as far off as they are (developer, 2026-11-13: sometimes the enemies sound much closer than they actually
are). Their voices (SA_EnemyVoice) faded in a straight line to silence at 30 m and went through walls untouched, so one
three rooms away was still half as loud as one beside you, and their steps used the player's own footstep attenuation.
Now both fall off like a real sound (Natural Sound: loud close by, quickly quieter with distance) and are muffled and
quieter with a wall between (occlusion: a trace from the listener; low-passed and turned down behind walls):
- SA_EnemyVoice: full within 1.5 m, silent past 30 m.
- SA_EnemyFootsteps (new): full within 1 m, silent past 20 m; set as every enemy Blueprint's footstep attenuation
  (re-run after setup_enemy_footsteps.py, which copies the player's).

Re-run to apply changes to the numbers below. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_enemy_sound_distance.py" -nullrhi -unattended
"""
import unreal

VOICE = ('/Game/Audio/SA_EnemyVoice', 150.0, 2850.0)
STEPS = ('/Game/Audio/SA_EnemyFootsteps', 100.0, 1900.0)
DB_AT_MAX = -50.0             # Natural Sound: how far down it is at the edge (dB)
OCCLUDED_VOLUME = 0.45        # behind a wall: this much of the volume
OCCLUDED_LOW_PASS = 1200.0    # and only below this frequency (Hz): muffled
ENEMIES = '/Game/Enemies'
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH sound distance] ' + str(msg))


def make(path, inner, falloff):
    asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not asset:
        folder, name = path.rsplit('/', 1)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    s = asset.get_editor_property('attenuation')
    s.set_editor_property('attenuate', True)
    s.set_editor_property('spatialize', True)
    # Binaural through Steam Audio (2026-11-16): above / below / behind on headphones; the sounds must be mono (make_enemy_sounds_mono.py)
    s.set_editor_property('spatialization_algorithm', unreal.SoundSpatializationAlgorithm.SPATIALIZATION_HRTF)
    s.set_editor_property('attenuation_shape', unreal.AttenuationShape.SPHERE)
    s.set_editor_property('attenuation_shape_extents', unreal.Vector(inner, 0, 0))
    s.set_editor_property('falloff_distance', falloff)
    s.set_editor_property('distance_algorithm', unreal.AttenuationDistanceModel.NATURAL_SOUND)
    s.set_editor_property('dBAttenuationAtMax', DB_AT_MAX)
    s.set_editor_property('enable_occlusion', True)
    s.set_editor_property('occlusion_trace_channel', unreal.CollisionChannel.ECC_VISIBILITY)
    s.set_editor_property('occlusion_volume_attenuation', OCCLUDED_VOLUME)
    s.set_editor_property('occlusion_low_pass_filter_frequency', OCCLUDED_LOW_PASS)
    s.set_editor_property('occlusion_interpolation_time', 0.15)
    asset.set_editor_property('attenuation', s)
    eal.save_loaded_asset(asset, False)
    out('%s: full within %.0f cm, silent past %.0f cm, natural, occluded' % (path.split('/')[-1], inner, inner + falloff))
    return asset


def main():
    make(*VOICE)
    steps = make(*STEPS)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(ENEMIES, recursive=True):
        if str(data.asset_class_path.asset_name) != 'Blueprint':
            continue
        bp = eal.load_asset(str(data.package_name))
        cls = bp.generated_class() if bp else None
        if not cls or not unreal.MathLibrary.class_is_child_of(cls, unreal.THEnemyCharacter):
            continue
        unreal.get_default_object(cls).get_editor_property('footstep_component').set_editor_property('attenuation', steps)
        eal.save_loaded_asset(bp, False)
        out('%s: footsteps use SA_EnemyFootsteps' % bp.get_name())


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

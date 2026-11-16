"""
Enemies' running footsteps (developer, 2026-11-13: zombies should make footstep noises when they are running): copies
the player's footstep sounds (per surface, the default set, the attenuation) from BP_FirstPersonCharacter's Footsteps
onto every enemy Blueprint's (ATHEnemyCharacter: steps only at running speed, and not a noise enemies hear; set in C++).

Re-run after changing the player's footstep sounds. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_enemy_footsteps.py" -nullrhi -unattended
"""
import unreal

PLAYER = '/Game/Blueprint/Character/BP_FirstPersonCharacter'
ENEMIES = '/Game/Enemies'
COPY = ['surface_sounds', 'default_sounds', 'attenuation']
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH steps] ' + str(msg))


def main():
    player_bp = eal.load_asset(PLAYER)
    source = unreal.get_default_object(player_bp.generated_class()).get_editor_property('footstep_component')
    out('player footsteps: %d surfaces, default walk %s, attenuation %s' % (len(source.get_editor_property('surface_sounds')),
        source.get_editor_property('default_sounds').get_editor_property('walk'), source.get_editor_property('attenuation')))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(ENEMIES, recursive=True):
        if str(data.asset_class_path.asset_name) != 'Blueprint':
            continue
        bp = eal.load_asset(str(data.package_name))
        cls = bp.generated_class() if bp else None
        if not cls or not unreal.MathLibrary.class_is_child_of(cls, unreal.THEnemyCharacter):
            continue
        target = unreal.get_default_object(cls).get_editor_property('footstep_component')
        for name in COPY:
            target.set_editor_property(name, source.get_editor_property(name))
        eal.save_loaded_asset(bp, False)
        out('%s: %d surfaces' % (bp.get_name(), len(target.get_editor_property('surface_sounds'))))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

"""
The director's tuning (developer, 2026-11-13: calm stretches building to peaks). Makes or updates
/Game/Enemies/Director/DA_Director (UTHDirectorSettings): waves of hunters (most often, 1-2), an ambusher pack (2-3) or a
stalker, as BP_Zombie; the rest of its numbers are the C++ defaults (a wave after 45-75 s of calm, a 25 s breather after
a peak, at most 12 enemies alive, brought in 15-35 m from the players out of their sight).

Re-run after changing the waves. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_director.py" -nullrhi -unattended
"""
import unreal

FOLDER = '/Game/Enemies/Director'
NAME = 'DA_Director'
BEHAVIORS = '/Game/Enemies/Behaviors/'
WAVES = [('DA_Hunter', 1, 2, 2.0), ('DA_Ambusher', 2, 3, 1.0), ('DA_Stalker', 1, 1, 0.5)]
ZOMBIE = '/Game/Enemies/BP_Zombie.BP_Zombie_C'
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH director] ' + str(msg))


def main():
    path = FOLDER + '/' + NAME
    asset = eal.load_asset(path) if eal.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.THDirectorSettings, unreal.DataAssetFactory())
    waves = []
    for behavior, low, high, weight in WAVES:
        wave = unreal.THDirectorWave()
        wave.set_editor_property('behavior', eal.load_asset(BEHAVIORS + behavior))
        wave.set_editor_property('count', unreal.IntPoint(low, high))
        wave.set_editor_property('weight', weight)
        waves.append(wave)
    asset.set_editor_property('waves', waves)
    asset.set_editor_property('enemy_class', eal.load_blueprint_class(ZOMBIE.split('.')[0]))
    eal.save_loaded_asset(asset, False)
    out('%s: %d waves, enemy %s' % (path, len(asset.get_editor_property('waves')), asset.get_editor_property('enemy_class')))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

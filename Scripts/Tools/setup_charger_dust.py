"""
The charger's wall crash dust (developer, 2026-11-16: no dust or debris pack; try Vefects' free Wind VFX). Its dust
specks (NS_Wind_Dust_Specs_01) loop: the charger spawns them where it hits the wall for Wall Crash Effect Time only, and
the specks then drift out their lives, a puff. The pack's effect as made, placed and timed by the code (nothing new
drawn). Tune the time and scale on DA_Charger.

Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_charger_dust.py" -nullrhi -unattended
"""
import unreal

DUST = '/Game/Vefects/Wind_VFX/VFX/Particles/NS_Wind_Dust_Specs_01'
CHARGER = '/Game/Enemies/Behaviors/DA_Charger'
TIME = 0.4      # s of spawning
SCALE = 0.6     # the pack's 2 m box, smaller: around the hit
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH charger dust] ' + str(msg))


try:
    charger = eal.load_asset(CHARGER)
    charger.set_editor_property('wall_crash_effect', eal.load_asset(DUST))
    charger.set_editor_property('wall_crash_effect_time', TIME)
    charger.set_editor_property('wall_crash_effect_scale', SCALE)
    eal.save_loaded_asset(charger, False)
    out('DA_Charger: wall crash effect %s, %.1f s, x%.1f' % (DUST.split('/')[-1], TIME, SCALE))
except Exception as e:
    out('ERROR %r' % e)

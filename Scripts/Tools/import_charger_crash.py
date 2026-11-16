"""
The charger's wall crash sounds (developer, 2026-11-12: from the Sound Effects bundle, ..\\Sound Effects, Humble Bundle,
free use): heavy door smacks and war club blows, imported into /Game/ThirdParty/SoundEffectsBundle/Crash, heard in the
world like an enemy's voice (SA_EnemyVoice), and set as DA_Charger's Wall Crash Sounds (one at random each crash).

Swap the picks in SOUNDS and re-run (reimports; replaces the list). Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/import_charger_crash.py" -nullrhi -unattended
"""
import os
import unreal

PACK = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\Sound Effects"
SOUNDS = [
    r"Doors\FL_GFX_Home_Fx_Door_Smack_01.wav",
    r"Doors\FL_GFX_Home_Fx_Door_Smack_02.wav",
    r"Medieval Fighting\FL_GFX_Medieval_Fighting_War_Club_01.wav",
    r"Medieval Fighting\FL_GFX_Medieval_Fighting_War_Club_02.wav",
]
DEST = "/Game/ThirdParty/SoundEffectsBundle/Crash"
ATTENUATION = "/Game/Audio/SA_EnemyVoice"
CHARGER = "/Game/Enemies/Behaviors/DA_Charger"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def out(msg):
    unreal.log_warning("[TH crash] " + str(msg))


def main():
    attenuation = eal.load_asset(ATTENUATION)
    waves = []
    for rel in SOUNDS:
        name = os.path.splitext(os.path.basename(rel))[0]
        task = unreal.AssetImportTask()
        task.filename = os.path.join(PACK, rel)
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tools.import_asset_tasks([task])
        wave = eal.load_asset("%s/%s" % (DEST, name))
        if not wave:
            out("FAILED to import " + rel)
            continue
        wave.set_editor_property("attenuation_settings", attenuation)
        eal.save_loaded_asset(wave, False)
        waves.append(wave)
        out("%s: %.2f s" % (name, wave.get_editor_property("duration")))
    charger = eal.load_asset(CHARGER)
    charger.set_editor_property("wall_crash_sounds", waves)
    eal.save_loaded_asset(charger, False)
    out("DA_Charger wall crash sounds: %d" % len(waves))


try:
    main()
except Exception as e:
    out("ERROR %r" % e)

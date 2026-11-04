"""Creates /Game/Maps/MainMenu: an empty level whose World Settings use ATHMenuGameMode (no character, the main menu).

Needs the full editor (level creation), so run it off-screen with the editor closed:
  UnrealEditor.exe "<abs>\\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\\create_main_menu_map.py" -RenderOffScreen -unattended -nosound
It quits the editor when done. Safe to re-run (it only sets the game mode on an existing map).
"""
import unreal

MAP = "/Game/Maps/MainMenu"
eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def out(msg):
    unreal.log_warning("[TH menu] " + str(msg))


if eal.does_asset_exist(MAP):
    out("loading existing %s: %s" % (MAP, les.load_level(MAP)))
else:
    out("new level %s: %s" % (MAP, les.new_level(MAP)))

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
settings = world.get_world_settings()
mode = unreal.load_class(None, "/Script/TacticalHorror.THMenuGameMode")
settings.set_editor_property("default_game_mode", mode)
out("game mode override = %s" % settings.get_editor_property("default_game_mode"))
out("saved=%s" % les.save_current_level())
out("done")
unreal.SystemLibrary.quit_editor()

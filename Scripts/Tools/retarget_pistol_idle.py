"""Retargets Lyra's UE4-mannequin pistol hip-fire idle onto the UE5 mannequin, for the pistol's lowered carry.

  Source: /Game/ThirdParty/Lyra/Characters/Mannequin_UE4/Animations/Locomotion/Pistol/MM_Pistol_Idle_Hipfire_UE4
  Result: /Game/Animations/ThirdPerson/G17/A_TP_G17_Lowered_Idle (overwritten if it exists: that LOSES the developer's
  hand pose keyed in it since. Retarget to another name and compare first.)
Uses Lyra's RTG_UE4Manny_UE5Manny. The batch retargeter needs Slate, so it can't run in the headless commandlet; run
it in a full editor drawn off-screen, which quits when done (editor closed first; copy the script to a file name that
doesn't start with "r", since "\\r" in the -ExecutePythonScript path is read as an escape):
  UnrealEditor.exe "<abs>\\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\\pistol_idle_rt.py" -RenderOffScreen -unattended -nosound
"""
import unreal

eal = unreal.EditorAssetLibrary
LYRA4 = "/Game/ThirdParty/Lyra/Characters/Mannequin_UE4/"
SOURCE = LYRA4 + "Animations/Locomotion/Pistol/MM_Pistol_Idle_Hipfire_UE4"
SOURCE_MESH = LYRA4 + "Meshes/SK_Mannequin"
TARGET_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
RETARGETER = LYRA4 + "Meshes/RTG_UE4Manny_UE5Manny"
DEST = "/Game/Animations/ThirdPerson/G17/A_TP_G17_Lowered_Idle"


def out(msg):
    unreal.log_warning("[TH retarget] " + msg)


source = eal.find_asset_data(SOURCE)
source_mesh = eal.load_asset(SOURCE_MESH)
target_mesh = eal.load_asset(TARGET_MESH)
retargeter = eal.load_asset(RETARGETER)
out("source=%s mesh=%s target=%s rtg=%s" % (source.is_valid(), source_mesh, target_mesh, retargeter))

if eal.does_asset_exist(DEST):
    eal.delete_asset(DEST)

new_assets = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
    [source], source_mesh, target_mesh, retargeter, "_UE4", "_TMPRT", "", "", False)
out("created %s" % [str(a.package_name) for a in new_assets])

for data in new_assets:
    path = str(data.package_name)
    if "MM_Pistol_Idle_Hipfire" in path:
        out("rename %s -> %s: %s" % (path, DEST, eal.rename_asset(path, DEST)))
        anim = eal.load_asset(DEST)
        out("skeleton=%s length=%.2f saved=%s" % (anim.get_editor_property("skeleton").get_path_name(),
                                                 unreal.AnimationLibrary.get_sequence_length(anim), eal.save_asset(DEST, False)))

out("done")
unreal.SystemLibrary.quit_editor()

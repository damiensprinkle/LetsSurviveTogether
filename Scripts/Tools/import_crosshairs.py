"""Imports the crosshair images from SourceArt/UI/Crosshairs (T_CH*.png) into /Game/UI/Crosshairs as UI textures
(no mipmaps, UI compression, never streamed, so they stay sharp at any size).

The crosshair list the game offers is in DefaultGame.ini ([/Script/TacticalHorror.THCrosshairWidget] Styles).
Safe to re-run (reimports). Run headless with the editor closed (see README.md).
"""
import glob
import os
import unreal

SRC = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\LetsSurviveTogether\SourceArt\UI\Crosshairs"
DEST = "/Game/UI/Crosshairs"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def out(msg):
    unreal.log_warning("[TH crosshair] " + str(msg))


for path in sorted(glob.glob(os.path.join(SRC, "T_CH*.png"))):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.factory = unreal.TextureFactory()
    tools.import_asset_tasks([task])
    tex = eal.load_asset(f"{DEST}/{name}")
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("srgb", True)
    out(f"{name} saved={eal.save_loaded_asset(tex, False)}")

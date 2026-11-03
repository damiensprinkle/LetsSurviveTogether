"""Gives the shell casings a metal material so they read as brass / copper instead of dark grey.

The ammo pack is spec/gloss: its *_DIFFUSE textures are dark grey grime and the colour is in *_SPECULAR (RGB = metal
colour, alpha = gloss). This builds /Game/Weapons/Ammo/M_Shell_Metal (base colour = ColorMap RGB, metallic 1,
roughness = (1 - ColorMap alpha) * RoughnessScale, normal = NormalMap), imports each pack's SPECULAR texture as
T_Shell_<key>_S, and points MI_Shell_45 / MI_Shell_762 at it. setup_shells.py builds the same when re-run.
Safe to re-run. Run headless with the editor closed (see README.md).
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
SRC = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\LetsSurviveTogether\SourceArt\AmmunitionMeshesTextures"
DEST = "/Game/Weapons/Ammo"
SPECULAR = {
    "45": r"Bullet 0.45\Textures\Bullet045_SPECULAR.png",
    "762": r"Bullet 7.62\Textures\Shell_Copper_SPECULAR.png",
}


def out(msg):
    unreal.log_warning("[TH shellmat] " + str(msg))


def import_texture(path, dest, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, path)
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.factory = unreal.TextureFactory()
    tools.import_asset_tasks([task])
    return eal.load_asset(f"{dest}/{name}")


def build_parent(default_color, default_normal):
    path = f"{DEST}/M_Shell_Metal"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = tools.create_asset("M_Shell_Metal", DEST, unreal.Material, unreal.MaterialFactoryNew())

    color = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, -100)
    color.set_editor_property("parameter_name", "ColorMap")
    color.set_editor_property("texture", default_color)
    mel.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    metallic = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 100)
    metallic.set_editor_property("parameter_name", "Metallic")
    metallic.set_editor_property("default_value", 1.0)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    gloss_to_rough = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -400, 200)
    mel.connect_material_expressions(color, "A", gloss_to_rough, "")
    scale = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 300)
    scale.set_editor_property("parameter_name", "RoughnessScale")
    scale.set_editor_property("default_value", 1.0)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 250)
    mel.connect_material_expressions(gloss_to_rough, "", rough, "A")
    mel.connect_material_expressions(scale, "", rough, "B")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    normal = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 400)
    normal.set_editor_property("parameter_name", "NormalMap")
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal.set_editor_property("texture", default_normal)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    out(f"M_Shell_Metal saved={eal.save_loaded_asset(mat, False)}")
    return mat


spec = {key: import_texture(path, f"{DEST}/{key}", f"T_Shell_{key}_S") for key, path in SPECULAR.items()}
parent = build_parent(spec["45"], eal.load_asset(f"{DEST}/45/T_Shell_45_N"))

for key in SPECULAR:
    mi = eal.load_asset(f"{DEST}/{key}/MI_Shell_{key}")
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_texture_parameter_value(mi, "ColorMap", spec[key])
    mel.set_material_instance_texture_parameter_value(mi, "NormalMap", eal.load_asset(f"{DEST}/{key}/T_Shell_{key}_N"))
    mel.update_material_instance(mi)
    out(f"MI_Shell_{key} -> M_Shell_Metal saved={eal.save_loaded_asset(mi, False)}")

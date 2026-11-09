"""Imports the Fab "worn overhead light" (THIRD_PARTY_ASSETS.csv) into /Game/ThirdParty/OverheadLight.

  Source: SourceArt/OverheadLight: SM_OverheadLight.obj (converted from the pack's model.dae by dae_to_obj.py, scale 1,
          pivot at the top centre so it hangs from where it's placed) and the pack's textures
  Result: Meshes/SM_OverheadLight (2 material slots), Textures/T_*, Materials/M_OverheadLight (shared PBR material with
          parameters) and MI_OverheadLight_Case / MI_OverheadLight_Tube. Glow = Emissive map x EmissiveTint x
          EmissiveStrength. The pack's tube emissive map is all black (shipped "off"), so the tube uses the white
          default instead and glows the colour of the 5000 K light. The mesh gives no light itself: BP_OverheadLight adds one.

Not headless: importing and creating assets syncs the Content Browser, which needs Slate (a headless run crashes).
Run in an off-screen editor that quits itself, editor closed:
  UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\Scripts\Tools\import_overhead_light.py" -RenderOffScreen -unattended -nosound
Safe to re-run: it re-imports and rebuilds the materials.
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt", "OverheadLight")
DST = "/Game/ThirdParty/OverheadLight"


def out(msg):
    unreal.log_warning("[TH Light] " + msg)


def import_files(files, folder):
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.filename = f
        task.destination_path = folder
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    return [p for t in tasks for p in t.imported_object_paths]


# Textures, with colour space and compression by map type
textures = sorted(os.path.join(SRC, "Textures", f) for f in os.listdir(os.path.join(SRC, "Textures")))
for path in import_files(textures, DST + "/Textures"):
    tex = eal.load_asset(path)
    name = tex.get_name().lower()
    if name.endswith("_normal"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif name.endswith(("_roughness", "_metallic", "_ao", "_opacity")):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    eal.save_loaded_asset(tex, False)
out("textures imported")

# Mesh
mesh_paths = [p for p in import_files([os.path.join(SRC, "SM_OverheadLight.obj")], DST + "/Meshes") if isinstance(eal.load_asset(p), unreal.StaticMesh)]
mesh = eal.load_asset(mesh_paths[0])
# Anything else the OBJ import made (default materials) is not used
for path in eal.list_assets(DST + "/Meshes", recursive=True):
    if not isinstance(eal.load_asset(path), unreal.StaticMesh):
        eal.delete_asset(path)
out("mesh %s" % mesh.get_path_name())

# One PBR material with texture parameters, two instances
MASTER = DST + "/Materials/M_OverheadLight"
if eal.does_asset_exist(MASTER):
    eal.delete_asset(MASTER)
master = tools.create_asset("M_OverheadLight", DST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
# The mesh is Nanite: without this the editor warns on every load, and a packaged game may not draw it
master.set_editor_property("used_with_nanite", True)


def tex_param(name, sampler, x, y, default):
    node = mel.create_material_expression(master, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    # The default texture must match the sampler type (a colour texture in a Normal sampler stops the whole material
    # compiling: it then renders as the plain default material, untextured and unlit). Use the pack's own maps.
    node.set_editor_property("texture", eal.load_asset(default))
    node.set_editor_property("sampler_type", sampler)
    return node


T = DST + "/Textures/T_light_case_1001_"


base = tex_param("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600, -300, T + "albedo")
normal = tex_param("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -600, 0, T + "normal")
rough = tex_param("Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -600, 250, T + "roughness")
metal = tex_param("Metallic", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -600, 500, T + "metallic")
ao = tex_param("AO", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -600, 750, T + "AO")
emissive = tex_param("Emissive", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600, 1000, "/Engine/EngineResources/WhiteSquareTexture")
tint = mel.create_material_expression(master, unreal.MaterialExpressionVectorParameter, -600, 1450)
tint.set_editor_property("parameter_name", "EmissiveTint")
tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
tinted = mel.create_material_expression(master, unreal.MaterialExpressionMultiply, -100, 1200)
strength = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -600, 1250)
strength.set_editor_property("parameter_name", "EmissiveStrength")
strength.set_editor_property("default_value", 0.0)
multiply = mel.create_material_expression(master, unreal.MaterialExpressionMultiply, -250, 1100)
mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
mel.connect_material_expressions(emissive, "RGB", multiply, "A")
mel.connect_material_expressions(strength, "", multiply, "B")
mel.connect_material_expressions(multiply, "", tinted, "A")
mel.connect_material_expressions(tint, "", tinted, "B")
mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(master)
eal.save_loaded_asset(master, False)

black = eal.load_asset("/Engine/EngineResources/Black")
SETS = {
    # instance: (texture prefix, emissive strength, emissive tint, use the pack's emissive map)
    "MI_OverheadLight_Case": ("T_light_case_1001", 0.0, unreal.LinearColor(1, 1, 1, 1), True),
    # The colour of the developer's 5000 K lights (255, 228, 206); the pack's map is black, so the white default is used
    "MI_OverheadLight_Tube": ("T_fluorescent_tube_1001", 60.0, unreal.LinearColor(1.0, 0.89, 0.81, 1), False),
}
instances = {}
for name, (prefix, glow, glow_tint, use_emissive_map) in SETS.items():
    path = DST + "/Materials/" + name
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, DST + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for param, suffix in (("BaseColor", "albedo"), ("Normal", "normal"), ("Roughness", "roughness"), ("Metallic", "metallic"),
                          ("AO", "AO"), ("Emissive", "emissive")):
        tex_path = "%s/Textures/%s_%s" % (DST, prefix, suffix)
        if param == "Emissive" and not use_emissive_map:
            # Explicitly white: leaving it unset keeps an override from an earlier run (the black map)
            mel.set_material_instance_texture_parameter_value(mi, param, eal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
            continue
        tex = eal.load_asset(tex_path) if eal.does_asset_exist(tex_path) else (black if param == "Emissive" else None)
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", glow)
    mel.set_material_instance_vector_parameter_value(mi, "EmissiveTint", glow_tint)
    eal.save_loaded_asset(mi, False)
    instances[prefix] = mi

# Material slots are named after the Collada materials
# (set on the mesh by index: editing the static_materials list only changes a copy)
for index, slot in enumerate(mesh.get_editor_property("static_materials")):
    name = str(slot.material_slot_name)
    for prefix, mi in instances.items():
        if prefix.lower() in name.lower():
            mesh.set_material(index, mi)
body = mesh.get_editor_property("body_setup")
body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
eal.save_loaded_asset(mesh, False)
box = mesh.get_bounding_box()
out("slots: %s" % [(str(s.material_slot_name), s.material_interface.get_name() if s.material_interface else None) for s in mesh.get_editor_property("static_materials")])
out("size %s cm (min %s max %s)" % (box.max - box.min, box.min, box.max))

unreal.SystemLibrary.quit_editor()

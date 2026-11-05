import unreal, os
def out(m): unreal.log_warning("[TH SH] " + str(m))
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
SRC = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\LetsSurviveTogether\SourceArt\AmmunitionMeshesTextures"
DEST = "/Game/Weapons/Ammo"
parent = eal.load_asset("/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial")
foley = "/Game/ThirdParty/WeaponsFoleySFX/SoundWav/"
attenuation = eal.load_asset("/Game/Audio/FootStep/FootstepSound/Sounds/FootstepsSoundAttenuation")

# name: (mesh fbx, diffuse png, normal png, real case length cm)
shells = {
    "45":  (r"Bullet 0.45\Mesh\Low-Poly\Shell (LP).FBX", r"Bullet 0.45\Textures\Bullet045_DIFFUSE.png", r"Bullet 0.45\Textures\Bullet045_NORMAL.png", 2.29),
    "762": (r"Bullet 7.62\Mesh\Shell.FBX", r"Bullet 7.62\Textures\Shell_Copper_DIFFUSE.png", r"Bullet 7.62\Textures\Shell_Copper_NORMAL.png", 3.9),
}
# weapon: (shell, eject offset from muzzle in aim space X fwd / Y right / Z up, launch velocity in the same space)
# Offsets measured from the rig: G17 slide (port ~6 cm behind the muzzle), M4 Bolt_j (45 cm behind).
# Velocities keep casings landing within a metre or two to the right and slightly behind.
# Collision: the casing actor has its own capsule (ATHShellCasing), so the mesh collision added below is optional;
# add_simple_collisions does nothing when this runs headless.
weapons = {
    "G17": ("45",  unreal.Vector(-6.0, 1.0, 1.2),  unreal.Vector(-40.0, 90.0, 110.0)),
    "M4":  ("762", unreal.Vector(-44.0, 1.5, 0.5), unreal.Vector(-50.0, 110.0, 90.0)),
}

def import_file(path, dest, name, options=None, factory=None):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, path); t.destination_path = dest; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True
    if options: t.options = options
    if factory: t.factory = factory
    tools.import_asset_tasks([t])
    return eal.load_asset(f"{dest}/{name}")

results = {}
# The editor subsystem is missing in a headless commandlet; the older library does the same job there
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
for key, (fbx, diff, nrm, real_len) in shells.items():
    d = f"{DEST}/{key}"
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    mesh = import_file(fbx, d, f"SM_Shell_{key}", ui)
    tex_d = import_file(diff, d, f"T_Shell_{key}_D", factory=unreal.TextureFactory())
    tex_n = import_file(nrm, d, f"T_Shell_{key}_N", factory=unreal.TextureFactory())
    tex_n.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex_n.set_editor_property("srgb", False)
    eal.save_loaded_asset(tex_n, False)

    mi_path = f"{d}/MI_Shell_{key}"
    mi = eal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else tools.create_asset(f"MI_Shell_{key}", d, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_texture_parameter_value(mi, "DiffuseColorMap", tex_d)
    mel.set_material_instance_scalar_parameter_value(mi, "DiffuseColorMapWeight", 1.0)
    mel.set_material_instance_vector_parameter_value(mi, "DiffuseColor", unreal.LinearColor(1, 1, 1, 1))
    mel.set_material_instance_texture_parameter_value(mi, "NormalMap", tex_n)
    mel.set_material_instance_scalar_parameter_value(mi, "NormalMapWeight", 1.0)
    mel.set_material_instance_scalar_parameter_value(mi, "Shininess", 20.0)
    # The pack is spec/gloss (colour in *_SPECULAR, *_DIFFUSE is dark grime): once shell_metal_material.py has built
    # M_Shell_Metal and the T_Shell_<key>_S textures, use those so the casings stay brass / copper
    metal = f"{DEST}/M_Shell_Metal"
    if eal.does_asset_exist(metal) and eal.does_asset_exist(f"{d}/T_Shell_{key}_S"):
        mi.set_editor_property("parent", eal.load_asset(metal))
        mel.set_material_instance_texture_parameter_value(mi, "ColorMap", eal.load_asset(f"{d}/T_Shell_{key}_S"))
        mel.set_material_instance_texture_parameter_value(mi, "NormalMap", tex_n)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)

    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, mi)
    sms.remove_collisions(mesh)
    sms.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.CAPSULE)
    eal.save_loaded_asset(mesh, False)

    ext = mesh.get_bounds().box_extent
    dims = [ext.x * 2, ext.y * 2, ext.z * 2]
    axis = dims.index(max(dims))
    scale = real_len / max(dims)
    if 0.7 < scale < 1.4:
        scale = 1.0  # already about the right size
    # Rotate the long axis onto +X (down the barrel)
    rot = [unreal.Rotator(), unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0)][axis]
    out(f"SM_Shell_{key}: size cm {[round(v, 2) for v in dims]} long axis {'XYZ'[axis]} scale {scale:.3f} rot {rot} slots {len(mesh.get_editor_property('static_materials'))}")
    results[key] = (mesh, rot, scale)

bounce = [eal.load_asset(f"{foley}Shells_0{i}_Wav") for i in range(1, 9)]
for s in bounce:
    out(f"{s.get_name()} {s.get_editor_property('duration'):.2f}s")

for n, (key, offset, velocity) in weapons.items():
    da = eal.load_asset(f"/Game/Weapons/Data/DA_{n}")
    mesh, rot, scale = results[key]
    da.set_editor_property("shell_mesh", mesh)
    da.set_editor_property("shell_mesh_rotation", rot)
    da.set_editor_property("shell_scale", scale)
    da.set_editor_property("shell_eject_offset", offset)
    da.set_editor_property("shell_eject_velocity", velocity)
    da.set_editor_property("shell_bounce_sounds", bounce)
    da.set_editor_property("shell_bounce_volume", 0.45)
    da.set_editor_property("shell_bounce_attenuation", attenuation)
    out(f"DA_{n} shells set, saved={eal.save_loaded_asset(da, False)}")

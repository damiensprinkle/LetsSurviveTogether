import unreal, os
def out(m): unreal.log_warning("[TH G17S] " + str(m))
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
G = "/Game/Weapons/G17/"
SRC = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\LetsSurviveTogether\SourceArt\G17_FPS_Pack\Glock 17\Textures"

# Textures from the pack (the FBX only embeds low-detail diffuse copies, no normal maps)
files = ["Glock_D.png", "Glock_N.png", "Glove_D.png", "Glove_N.png", "Hand_D.jpg", "Hand_N.png"]
tasks = []
for f in files:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f); t.destination_path = G + "Textures"
    t.destination_name = "T_" + os.path.splitext(f)[0]
    t.automated = True; t.replace_existing = True; t.save = True; t.factory = unreal.TextureFactory()
    tasks.append(t)
tools.import_asset_tasks(tasks)
tex = lambda n: eal.load_asset(G + "Textures/T_" + n)
for n in ["Glock_N", "Glove_N", "Hand_N"]:
    nt = tex(n)
    nt.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    nt.set_editor_property("srgb", False)
    eal.save_loaded_asset(nt, False)

# Material instances on the same parent as the M4's
parent = eal.load_asset("/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial")
mis = {}
for slot, base in [("Glock_D", "Glock"), ("Glove_D", "Glove"), ("Hand_D", "Hand")]:
    path = G + "Materials/MI_" + base
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset("MI_" + base, G + "Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_texture_parameter_value(mi, "DiffuseColorMap", tex(base + "_D"))
    mel.set_material_instance_scalar_parameter_value(mi, "DiffuseColorMapWeight", 1.0)
    mel.set_material_instance_vector_parameter_value(mi, "DiffuseColor", unreal.LinearColor(1, 1, 1, 1))
    mel.set_material_instance_texture_parameter_value(mi, "NormalMap", tex(base + "_N"))
    mel.set_material_instance_scalar_parameter_value(mi, "NormalMapWeight", 1.0)
    mel.set_material_instance_scalar_parameter_value(mi, "Shininess", 2.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    mis[slot] = mi
mesh = eal.load_asset(G + "Glock17")
slots = mesh.get_editor_property("materials")
for s in slots:
    name = str(s.get_editor_property("material_slot_name"))
    if name in mis:
        s.set_editor_property("material_interface", mis[name])
mesh.set_editor_property("materials", slots)
out(f"mesh slots={[(str(s.get_editor_property('material_slot_name')), s.get_editor_property('material_interface').get_name()) for s in slots]} saved={eal.save_loaded_asset(mesh, False)}")

# Data asset: the placeholder pistol's tuning, the G17's presentation
DA = "/Game/Weapons/Data/DA_G17"
if not eal.does_asset_exist(DA):
    eal.duplicate_asset("/Game/Weapons/Data/DA_Pistol", DA)
da = eal.load_asset(DA)
anim = lambda n: eal.load_asset(G + "Glock17_Anim_" + n)
da.set_editor_property("display_name", "G17")
da.set_editor_property("magazine_size", 17)
for prop, n in [("idle_animation", "Idle"), ("equip_animation", "Draw"), ("fire_animation", "Fire"), ("reload_animation", "Reload"), ("reload_empty_animation", "ReloadEmpty")]:
    da.set_editor_property(prop, anim(n))
da.set_editor_property("reload_duration", anim("Reload").get_play_length())
da.set_editor_property("equip_duration", anim("Draw").get_play_length())
# Pack's fps_camera_j (1.29, -11.0, 153.0) inverted, same convention as the M4
da.set_editor_property("camera_hold_offset", unreal.Transform([11.0, 1.29, -153.0], unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), [1, 1, 1]))
idle = anim("Idle")
pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, unreal.AnimPoseEvaluationOptions())
main = unreal.AnimPoseExtensions.get_bone_pose(pose, "main_j", unreal.AnimPoseSpaces.WORLD)
muzzle_mesh = unreal.Vector(-4.83, 22.6, 148.0)  # front of the slide, bore height; check with a render
da.set_editor_property("muzzle_socket_name", "main_j")
da.set_editor_property("muzzle_offset", main.inverse_transform_location(muzzle_mesh))
out(f"DA_G17 muzzle offset {da.get_editor_property('muzzle_offset')} saved={eal.save_loaded_asset(da, False)}")

# Weapon Blueprint
path = "/Game/Weapons/Blueprints/BP_Weapon_G17"
if eal.does_asset_exist(path):
    bp = eal.load_asset(path)
else:
    f = unreal.BlueprintFactory(); f.set_editor_property("parent_class", unreal.THWeaponBase)
    bp = tools.create_asset("BP_Weapon_G17", "/Game/Weapons/Blueprints", None, f)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("weapon_data", da)
cdo.get_editor_property("mesh").set_editor_property("skeletal_mesh_asset", mesh)
out(f"BP_Weapon_G17 saved={eal.save_loaded_asset(bp, False)}")

# Loadout: G17 replaces the placeholder pistol (which stays in the project)
char = eal.load_asset("/Game/Blueprint/Character/BP_FirstPersonCharacter")
wc = unreal.get_default_object(char.generated_class()).get_editor_property("weapon_component")
wc.set_editor_property("default_loadout", [eal.load_asset("/Game/Weapons/Blueprints/BP_Weapon_M4").generated_class(), bp.generated_class()])
out(f"loadout={[c.get_name() for c in wc.get_editor_property('default_loadout')]} saved={eal.save_loaded_asset(char, False)}")

# Reload timing: magazine and slide motion relative to the gun
for name in ["Reload", "ReloadEmpty"]:
    a = anim(name)
    L = a.get_play_length(); n = int(L * 30)
    out(f"== {name} {L:.2f}s  t | mag1 | mag2 | slide")
    base = None
    for i in range(n + 1):
        t = L * i / n
        p = unreal.AnimPoseExtensions.get_anim_pose_at_time(a, t, unreal.AnimPoseEvaluationOptions())
        g = lambda b: unreal.AnimPoseExtensions.get_bone_pose(p, b, unreal.AnimPoseSpaces.WORLD)
        gun = g("main_j")
        row = [gun.inverse_transform_location(g(b).translation) for b in ("mag1_j", "mag2_j", "slide_j")]
        if base is None: base = row
        out(f"  {t:5.2f} | {(row[0]-base[0]).length():6.1f} | {(row[1]-base[1]).length():6.1f} | {(row[2]-base[2]).length():5.2f}")

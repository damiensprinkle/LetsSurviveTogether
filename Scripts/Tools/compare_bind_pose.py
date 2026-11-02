import unreal
def out(m): unreal.log_warning("[TH Bind] " + str(m))
eal = unreal.EditorAssetLibrary
A = unreal.AnimPoseExtensions
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
TMP = "/Game/Temp_G17Bind"
ui = unreal.FbxImportUI()
ui.import_mesh = True; ui.import_as_skeletal = True
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
ui.import_animations = False; ui.import_materials = False; ui.import_textures = False; ui.create_physics_asset = False
ui.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)
t = unreal.AssetImportTask()
t.filename = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\LetsSurviveTogether\SourceArt\G17_FPS_Pack\Glock 17\Glock17.fbx"
t.destination_path = TMP; t.automated = True; t.replace_existing = True; t.save = False; t.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
bind_mesh = [eal.load_asset(p) for p in eal.list_assets(TMP) if isinstance(eal.load_asset(p), unreal.SkeletalMesh)][0]
pb = A.get_reference_pose(bind_mesh.get_editor_property("skeleton"))
po = A.get_reference_pose(eal.load_asset("/Game/Weapons/G17/Glock17").get_editor_property("skeleton"))
names = [str(n) for n in A.get_bone_names(po)]
bnames = set(str(n) for n in A.get_bone_names(pb))
for b in names:
    if b not in bnames:
        out(f"{b} missing in bind"); continue
    lo, lb = A.get_bone_pose(po, b, unreal.AnimPoseSpaces.LOCAL), A.get_bone_pose(pb, b, unreal.AnimPoseSpaces.LOCAL)
    dt = (lo.translation - lb.translation).length(); dr = lo.rotation.angular_distance(lb.rotation) * 57.2958
    if dt > 0.01 or dr > 0.1:
        out(f"{b:20} dT={dt:6.2f} dR={dr:6.1f}  t0={lo.translation} bind={lb.translation}  t0rot={lo.rotation.rotator()} bindrot={lb.rotation.rotator()}")
out("done")

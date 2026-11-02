import unreal
def out(m): unreal.log_warning("[TH G17] " + str(m))
eal = unreal.EditorAssetLibrary
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
src = r"C:\Users\Damien Sprinkle\Documents\Unreal Projects\LetsSurviveTogether\SourceArt\G17_FPS_Pack\Glock 17\Glock17.fbx"
dest = "/Game/Weapons/G17"
ui = unreal.FbxImportUI()
ui.import_mesh = True
ui.import_as_skeletal = True
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
ui.import_animations = True
ui.import_materials = True
ui.import_textures = False
ui.create_physics_asset = False
ui.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", True)
ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
ui.anim_sequence_import_data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
task = unreal.AssetImportTask()
task.filename = src; task.destination_path = dest
task.automated = True; task.replace_existing = True; task.save = True; task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
out("save_directory: " + str(eal.save_directory(dest, only_if_is_dirty=False, recursive=True)))
for p in eal.list_assets(dest, recursive=True):
    a = eal.load_asset(p)
    info = f"{type(a).__name__:24} {p}"
    if isinstance(a, unreal.SkeletalMesh):
        b = a.get_bounds()
        info += f"  origin={b.origin} extent={b.box_extent} slots={[str(s.get_editor_property('material_slot_name')) for s in a.get_editor_property('materials')]}"
    if isinstance(a, unreal.AnimSequence):
        info += f"  length={a.get_play_length():.2f}s"
    if isinstance(a, unreal.MaterialInstance):
        par = a.get_editor_property("parent")
        info += f"  parent={par.get_path_name() if par else None} tex={list(unreal.MaterialEditingLibrary.get_texture_parameter_names(a))}"
    out(info)
anims = [eal.load_asset(p) for p in eal.list_assets(dest, recursive=True) if isinstance(eal.load_asset(p), unreal.AnimSequence)]
if anims:
    a = anims[0]
    names = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(a)]
    out(f"{len(names)} bones: " + ", ".join(names))
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(a, 0.0, unreal.AnimPoseEvaluationOptions())
    for b in names:
        if any(k in b.lower() for k in ["cam", "main", "slide", "mag", "muzzle", "barrel"]):
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD)
            out(f"   {b}: {t.translation} rot={t.rotation.rotator()}")

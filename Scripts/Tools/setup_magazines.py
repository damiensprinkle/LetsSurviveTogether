"""Third-person reload magazines: cuts each gun's magazine out of its first-person rig as a static mesh, and fills the
*Third Person Reload* section of DA_M4 / DA_G17.

  /Game/Weapons/M4_T0/SM_M4_Magazine, /Game/Weapons/G17/SM_G17_Magazine
      The triangles skinned to the magazine bone (and its children, the rounds), in that bone's space, with the gun's
      materials and a box collision. The vendor's geometry, only split off.
  ThirdPersonMagazineBone        the magazine bone in the gun (hidden on the third-person gun while the magazine is out)
  ThirdPersonMagazineInPalm      where the first-person reload holds the magazine relative to the left palm (knuckles
                                 frame, as FTHHandShape), so the body's left hand can hold the new one the same way
  MagazineDropAt / GrabAt / SeatAt  fractions of the reload, from the body's Lyra reload: the hand leaves the gun,
                                 reaches the waist, and seats the new magazine

Needs Geometry Script: run with -EnablePlugins=GeometryScripting. Safe to re-run (it replaces the meshes).
"""
import unreal

eal = unreal.EditorAssetLibrary
AP = unreal.AnimPoseExtensions
WS = unreal.AnimPoseSpaces.WORLD
BW = unreal.GeometryScript_BoneWeights
LU = unreal.GeometryScript_List


def out(msg):
    unreal.log_warning("[TH Mag] " + msg)


GUNS = {
    # data asset: (skeletal mesh, magazine joint in the gun, new mesh, FP reload time the left hand holds a magazine,
    #              drop, grab, seat as fractions of the reload)
    "DA_M4": ("/Game/Weapons/M4_T0/M4", "Mag1_j", "/Game/Weapons/M4_T0/SM_M4_Magazine", 1.4, 0.15, 0.27, 0.57),
    "DA_G17": ("/Game/Weapons/G17/Glock17", "Mag2_j", "/Game/Weapons/G17/SM_G17_Magazine", 1.1, 0.08, 0.28, 0.52),
}


def palm_transform(pose, wrist, index, middle, pinky):
    """FTHHandShape: X from the wrist to the middle knuckle, Y from the pinky to the index knuckle, at the knuckles' centre."""
    w = AP.get_bone_pose(pose, wrist, WS).translation
    i = AP.get_bone_pose(pose, index, WS).translation
    m = AP.get_bone_pose(pose, middle, WS).translation
    p = AP.get_bone_pose(pose, pinky, WS).translation
    rot = unreal.MathLibrary.make_rot_from_xy(m - w, i - p)
    return unreal.Transform((i + m + p) * (1.0 / 3.0), rot, unreal.Vector(1, 1, 1))


def pick(result, cls):
    """Geometry Script calls return tuples whose order varies by version: take the value of the wanted type."""
    for value in (result if isinstance(result, tuple) else (result,)):
        if isinstance(value, cls):
            return value
    raise RuntimeError("no %s in %s" % (cls.__name__, result))


def to_array(index_list):
    return list(LU.convert_index_list_to_array(index_list))


def cut_magazine(skm_path, joint, new_path):
    skm = eal.load_asset(skm_path)
    mesh = unreal.DynamicMesh()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_index = 0
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(skm, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)

    _, valid, joint_index = BW.get_bone_index(mesh, joint)
    _, _, children = BW.get_bone_children(mesh, joint, True)
    keep_bones = {joint_index} | {c.index for c in children}
    _, _, info = BW.get_bone_info(mesh, joint)
    out("%s: joint %s index %d (valid %s), %d bones in the magazine" % (skm_path, joint, joint_index, valid, len(keep_bones)))

    MQ = unreal.GeometryScript_MeshQueries
    vertex_ids = pick(MQ.get_all_vertex_i_ds(mesh), unreal.GeometryScriptIndexList)
    in_mag = {}
    for vid in to_array(vertex_ids):
        _, weight, ok = BW.get_largest_vertex_bone_weight(mesh, vid)
        in_mag[vid] = ok and weight.bone_index in keep_bones

    triangles = pick(MQ.get_all_triangle_indices(mesh, False), unreal.GeometryScriptTriangleList)
    tri_ids = to_array(pick(MQ.get_all_triangle_i_ds(mesh), unreal.GeometryScriptIndexList))
    tris = list(LU.convert_triangle_list_to_array(triangles))
    remove = [tid for tid, t in zip(tri_ids, tris) if not (in_mag.get(t.x) and in_mag.get(t.y) and in_mag.get(t.z))]
    out("  keeping %d of %d triangles" % (len(tri_ids) - len(remove), len(tri_ids)))
    remove_list = LU.convert_array_to_index_list(remove, unreal.GeometryScriptIndexType.TRIANGLE)
    unreal.GeometryScript_MeshEdits.delete_triangles_from_mesh(mesh, remove_list)

    # Into the joint's space, so the mesh placed at the joint sits where the magazine was
    unreal.GeometryScript_MeshTransforms.inverse_transform_mesh(mesh, info.world_transform)

    if eal.does_asset_exist(new_path):
        eal.delete_asset(new_path)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.enable_recompute_normals = False
    options.enable_recompute_tangents = False
    options.enable_collision = False
    static_mesh, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, new_path, options)

    # The cut keeps the skeletal mesh's material IDs: give the new mesh the same material list
    materials = [m.get_editor_property("material_interface") for m in skm.get_editor_property("materials")]
    static_materials = []
    for index, material in enumerate(materials):
        sm = unreal.StaticMaterial()
        sm.set_editor_property("material_interface", material)
        sm.set_editor_property("material_slot_name", skm.get_editor_property("materials")[index].get_editor_property("material_slot_name"))
        static_materials.append(sm)
    static_mesh.set_editor_property("static_materials", static_materials)
    # No collision on the asset: the dropped magazine actor fits its own box to the mesh bounds
    eal.save_loaded_asset(static_mesh)
    b = static_mesh.get_bounding_box()
    out("  saved %s, bounds %s .. %s" % (new_path, b.min, b.max))
    return static_mesh


for name, (skm_path, joint, new_path, hold_time, drop, grab, seat) in GUNS.items():
    data = eal.load_asset("/Game/Weapons/Data/" + name)
    mag_mesh = cut_magazine(skm_path, joint, new_path)

    # How the first-person reload holds a magazine in the left hand: the magazine joint nearest the palm at hold_time
    reload_anim = data.get_editor_property("reload_animation")
    pose = AP.get_anim_pose_at_time(reload_anim, hold_time, unreal.AnimPoseEvaluationOptions())
    palm = palm_transform(pose, "LeftHand", "LeftHandIndex1", "LeftHandMiddle1", "LeftHandPinky1")
    best = None
    for bone in [str(b) for b in AP.get_bone_names(pose)]:
        if not bone.lower().startswith("mag") or not bone.endswith("_j") or "bullet" in bone.lower():
            continue
        relative = AP.get_bone_pose(pose, bone, WS).make_relative(palm) if hasattr(unreal.Transform, "make_relative") else None
        if relative is None:
            relative = unreal.MathLibrary.make_relative_transform(AP.get_bone_pose(pose, bone, WS), palm)
        distance = relative.translation.length()
        out("  %s at %.2fs: %s is %.1f cm from the left palm" % (reload_anim.get_name(), hold_time, bone, distance))
        if best is None or distance < best[1]:
            best = (relative, distance, bone)
    out("  held magazine: %s (%.1f cm)" % (best[2], best[1]))

    data.set_editor_property("third_person_magazine_bone", joint)
    data.set_editor_property("third_person_magazine_mesh", mag_mesh)
    data.set_editor_property("third_person_magazine_in_palm", best[0])
    data.set_editor_property("magazine_drop_at", drop)
    data.set_editor_property("magazine_grab_at", grab)
    data.set_editor_property("magazine_seat_at", seat)
    eal.save_loaded_asset(data)
    out(name + " saved")

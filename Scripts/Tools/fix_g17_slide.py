import unreal
def out(m): unreal.log_warning("[TH Fix] " + str(m))
eal = unreal.EditorAssetLibrary
A = unreal.AnimPoseExtensions
M = unreal.MathLibrary
W, L = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
G = "/Game/Weapons/G17/"
# The G17 is imported with "Use T0 As Ref Pose" (the arms need it), but frame 0 of the FBX has the slide locked
# back while the gun was modelled with it there too, so the slide renders 4.6 cm back at rest. These bones' true
# rest (bind) locals, parent-relative, from an import without T0. Their parents (Main_j) are unaffected.
BIND_LOCAL = {
    "Slide_j": unreal.Vector(0.0, -2.28, -1.47),
    "SlideRelease_j": unreal.Vector(1.39, -0.99, -0.86),
    "Mag1": unreal.Vector(0.0, 0.0, 0.0),
}
PARENT = {"Slide_j": "Main_j", "SlideRelease_j": "Main_j", "Mag1": "Main_j"}
skel = eal.load_asset(G + "Glock17").get_editor_property("skeleton")
ref = A.get_reference_pose(skel)
fix = {}
for b, t in BIND_LOCAL.items():
    t0_local = A.get_bone_pose(ref, b, L)
    p_ref = A.get_bone_pose(ref, PARENT[b], W)
    bind_local = unreal.Transform(t, unreal.Rotator(0, 0, 0), [1, 1, 1])
    t0_cs = M.compose_transforms(t0_local, p_ref)
    bind_cs = M.compose_transforms(bind_local, p_ref)
    # Skin = inv(Ref) * Anim. Want inv(T0) * Anim' == inv(Bind) * Anim  =>  Anim' = T0 * inv(Bind) * Anim
    fix[b] = M.compose_transforms(t0_cs, M.invert_transform(bind_cs))
opts = unreal.AnimPoseEvaluationOptions()
for n in ["Idle", "Draw", "Fire", "Holster", "Reload", "ReloadEmpty"]:
    a = eal.load_asset(G + "Glock17_Anim_" + n)
    keys = unreal.AnimationLibrary.get_num_keys(a)
    length = a.get_play_length()
    data = {b: ([], [], []) for b in fix}
    for i in range(keys):
        pose = A.get_anim_pose_at_time(a, length * i / max(1, keys - 1), opts)
        for b, f in fix.items():
            cs = M.compose_transforms(f, A.get_bone_pose(pose, b, W))
            local = M.compose_transforms(cs, M.invert_transform(A.get_bone_pose(pose, PARENT[b], W)))
            data[b][0].append(local.translation)
            data[b][1].append(local.rotation)
            data[b][2].append(local.scale3d)
    ctrl = a.get_editor_property("controller")
    for b, (p, r, s) in data.items():
        ok = ctrl.set_bone_track_keys(b, p, r, s, False)
        if not ok:
            out(f"{n}: set_bone_track_keys failed for {b}")
    pose = A.get_anim_pose_at_time(a, 0.0, opts)
    rel = A.get_bone_pose(pose, "Main_j", W).inverse_transform_location(A.get_bone_pose(pose, "Slide_j", W).translation)
    out(f"{n}: {keys} keys, slide_j at t0 now {rel.z:.2f} (renders {rel.z + 1.47:.2f} cm ahead of the modelled slide), saved={eal.save_loaded_asset(a, False)}")

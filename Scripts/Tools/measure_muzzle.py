import unreal
def out(m): unreal.log_warning("[TH Muz] " + str(m))
eal = unreal.EditorAssetLibrary
A = unreal.AnimPoseExtensions
mesh = eal.load_asset("/Game/Weapons/G17/Glock17")
dm = unreal.DynamicMesh()
res = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
out(f"copy result: {res}")
r = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, True)
vl = r[1]
out(f"list type {type(vl)} {[a for a in dir(vl) if not a.startswith(chr(95))][:20]}")
pts = list(vl.convert_vector_list_to_array())
out(f"{len(pts)} verts")
ref = A.get_reference_pose(mesh.get_editor_property("skeleton"))
mj = A.get_bone_pose(ref, "Main_j", unreal.AnimPoseSpaces.WORLD)
loc = [mj.inverse_transform_location(p) for p in pts]
# Gun body: near the bore axis laterally
gun = [p for p in loc if abs(p.x) < 2.0 and -6.0 < p.y < 4.0 and -15.0 < p.z < 25.0]
out(f"gun-ish verts {len(gun)}, z range {min(p.z for p in gun):.2f}..{max(p.z for p in gun):.2f}, y range {min(p.y for p in gun):.2f}..{max(p.y for p in gun):.2f}")
zmax = max(p.z for p in gun)
for band in (0.3, 1.0, 3.0, 6.0):
    f = [p for p in gun if p.z > zmax - band]
    out(f"front {band}cm: n={len(f)} x {min(p.x for p in f):.2f}..{max(p.x for p in f):.2f}  y {min(p.y for p in f):.2f}..{max(p.y for p in f):.2f}  mean ({sum(p.x for p in f)/len(f):.2f}, {sum(p.y for p in f)/len(f):.2f})")
# slide extent (modelled locked back) for reference: upper-most verts along the length
top = min(p.y for p in gun)
out(f"top of gun y={top:.2f}")




f = sorted([p for p in gun if p.z > zmax - 0.6], key=lambda p: p.y)
for p in f[::3]:
    out(f"   y={p.y:6.2f} x={p.x:6.2f} z={p.z:6.2f}")

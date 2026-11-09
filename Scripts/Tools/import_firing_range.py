"""Imports the Fab firing range (THIRD_PARTY_ASSETS.csv) as a laid-out level.

  Source:  ../Range/shooting_range.fbx (+ ../Range/Tex), downloaded by the developer, outside the project
  Assets:  /Game/ThirdParty/FiringRange (meshes, materials, textures: the pack, kept as it comes)
  Level:   /Game/Maps/L_FiringRange (ours: every piece where the creator placed it; edit this one)

Uses Interchange's scene import, which keeps the FBX's 345 placed objects and shares the 60 meshes between them.
Run headless with the editor closed (see README.md), twice: first the import, then TH_STEP=fixup (a fresh run that
loads the level: names the meshes SM_<piece> in Meshes/, turns off the emissive the FBX put on every material, removes
the one empty mesh). TH_REPLACE=1 deletes and re-imports both.
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX = os.path.normpath(os.path.join(PROJECT, "..", "Range", "shooting_range.fbx"))
ASSETS = "/Game/ThirdParty/FiringRange"
LEVEL = "/Game/Maps/L_FiringRange"


def out(msg):
    unreal.log_warning("[TH Range] " + msg)


def fix_up():
    """After the import: name the meshes after their pieces, stop materials glowing, drop the one broken mesh."""
    import collections
    import re
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    levels.load_level(LEVEL)
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # 1. The importer named every mesh Scene_ncl_N. Name each after the pieces placed with it: SM_<piece>.
    use = collections.defaultdict(list)
    for actor in actor_sub.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.static_mesh
            if mesh:
                use[mesh].append(actor)
    renames = []
    taken = set()
    for mesh, actors in use.items():
        if not mesh.get_name().startswith("Scene"):
            continue
        bases = collections.Counter(re.sub(r"(__\d+_)+$", "", a.get_actor_label()) for a in actors)
        base = bases.most_common(1)[0][0]
        if len(bases) > 1:
            # Target, Target_1 ... -> Target; FireRange_NumberSign1..6 -> FireRange_NumberSign
            base = os.path.commonprefix(list(bases)).rstrip("_0123456789") or base
        name = "SM_" + base
        suffix = 2
        while name in taken or eal.does_asset_exist("%s/%s" % (ASSETS, name)):
            name = "SM_%s_%d" % (base, suffix)
            suffix += 1
        taken.add(name)
        if mesh.get_num_lods() == 0 or mesh.get_num_triangles(0) == 0:
            # Crate_Debris01 came in empty ("Bad MeshDescription"): remove its piece and the mesh
            for actor in actors:
                out("removing %s (its mesh %s is empty)" % (actor.get_actor_label(), mesh.get_name()))
                actor_sub.destroy_actor(actor)
            continue
        renames.append(unreal.AssetRenameData(mesh, ASSETS + "/Meshes", name))
    if renames:
        out("renaming %d meshes: %s" % (len(renames), ", ".join(str(r.new_name) for r in renames)))
        unreal.AssetToolsHelpers.get_asset_tools().rename_assets(renames)

    # 2. The FBX feeds every colour texture into emissive too (a 3ds Max export habit): the whole range would light
    #    itself. Only ModularParts04 has a real emissive map (_EMM); switch emissive off on the rest.
    for path in eal.list_assets(ASSETS, recursive=True):
        mi = eal.load_asset(path)
        if isinstance(mi, unreal.MaterialInstanceConstant):
            emissive = None
            for tp in mi.get_editor_property("texture_parameter_values"):
                if str(tp.parameter_info.name) == "EmissiveColorMap" and tp.parameter_value:
                    emissive = tp.parameter_value.get_name()
            keep = bool(emissive) and emissive.endswith("_EMM")
            unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "EmissiveColorMapWeight", 1.0 if keep else 0.0)
            if not keep:
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(0, 0, 0, 1))
            out("%s: emissive %s" % (mi.get_name(), "kept (%s)" % emissive if keep else "off"))

    levels.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    # 3. Remove the empty mesh and the redirectors the renames left (the level, their only user, is saved)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    for path in eal.list_assets(ASSETS, recursive=False):
        package = path.split(".")[0]
        data = eal.find_asset_data(path)
        kind = str(data.asset_class_path.asset_name)
        empty_mesh = kind == "StaticMesh" and package.split("/")[-1].startswith("Scene")
        if kind == "ObjectRedirector" or empty_mesh:
            refs = [str(r) for r in (registry.get_referencers(package, opts) or [])]
            if refs:
                out("KEEP %s: used by %s" % (package, refs))
            else:
                eal.delete_asset(package)
                out("deleted %s %s" % (kind, package))


def make_playable():
    """Collision on every piece, a player start behind the middle shooting counter facing downrange, placeholder lights."""
    import math
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    levels.load_level(LEVEL)
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # 1. The kit's meshes came without usable collision: the visible geometry becomes the collision. Nothing in the
    #    room moves, so complex-as-simple is fine (and bullets hit walls and counters exactly).
    for path in eal.list_assets(ASSETS + "/Meshes", recursive=True):
        mesh = eal.load_asset(path)
        if isinstance(mesh, unreal.StaticMesh):
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            mesh.set_editor_property("body_setup", body)
            eal.save_loaded_asset(mesh, False)
    out("collision: complex as simple on every mesh in Meshes/")

    actors = actor_sub.get_all_level_actors()
    for actor in actors:
        if isinstance(actor, unreal.StaticMeshActor):
            actor.static_mesh_component.set_collision_profile_name("BlockAll")

    def centre(prefix):
        points = [a.get_actor_bounds(False)[0] for a in actors if isinstance(a, unreal.StaticMeshActor) and a.get_actor_label().startswith(prefix)]
        return sum(points, unreal.Vector(0, 0, 0)) * (1.0 / len(points)), points

    counters, counter_points = centre("FireRange_Counter")
    targets, _ = centre("Target")
    floors = [a for a in actors if isinstance(a, unreal.StaticMeshActor) and a.get_actor_label().startswith("FireRange_FloorPlane")]
    floor_top = max(o.z + e.z for o, e in (f.get_actor_bounds(False) for f in floors))
    downrange = unreal.Vector(targets.x - counters.x, targets.y - counters.y, 0)
    downrange = downrange * (1.0 / max(downrange.length(), 1.0))
    yaw = math.degrees(math.atan2(downrange.y, downrange.x))

    # 2. Player start 2.5 m behind the counters' middle, facing the targets
    for actor in actors:
        if isinstance(actor, unreal.PlayerStart):
            actor_sub.destroy_actor(actor)
    start_location = unreal.Vector(counters.x - downrange.x * 250, counters.y - downrange.y * 250, floor_top + 100)
    start = actor_sub.spawn_actor_from_class(unreal.PlayerStart, start_location, unreal.Rotator(0, 0, yaw))
    start.set_actor_label("PlayerStart")
    out("player start at %s facing yaw %.0f (floor top Z %.0f, counters at %s, targets at %s)" % (start_location, yaw, floor_top, counters, targets))

    # 3. Placeholder lights (the developer will light it properly): a row down the middle of the range
    for actor in actors:
        if isinstance(actor, unreal.PointLight) and actor.get_actor_label().startswith("TH_PlaceholderLight"):
            actor_sub.destroy_actor(actor)
    ceiling = max(a.get_actor_bounds(False)[0].z + a.get_actor_bounds(False)[1].z for a in actors
                  if isinstance(a, unreal.StaticMeshActor) and "Ceilling" in a.get_actor_label())
    length = (targets - counters).length() + 600
    count = max(2, int(length / 600))
    for i in range(count + 1):
        along = -400 + length * i / count
        location = unreal.Vector(counters.x + downrange.x * along, counters.y + downrange.y * along, min(ceiling, floor_top + 600) - 80)
        light = actor_sub.spawn_actor_from_class(unreal.PointLight, location)
        light.set_actor_label("TH_PlaceholderLight%02d" % (i + 1))
        component = light.get_component_by_class(unreal.PointLightComponent)
        component.set_editor_property("intensity", 5000.0)
        component.set_editor_property("attenuation_radius", 1200.0)
    out("%d placeholder lights, every %.0f cm" % (count + 1, length / count))
    levels.save_current_level()


if os.environ.get("TH_STEP") == "fixup":
    fix_up()
    out("finished")
    raise SystemExit
if os.environ.get("TH_STEP") == "playable":
    make_playable()
    out("finished")
    raise SystemExit

if not os.path.exists(FBX):
    raise RuntimeError("missing " + FBX)
if os.environ.get("TH_REPLACE") == "1":
    for path in (LEVEL, ASSETS):
        if eal.does_asset_exist(path) or eal.does_directory_exist(path):
            (eal.delete_directory if path == ASSETS else eal.delete_asset)(path)
if eal.does_asset_exist(LEVEL):
    raise RuntimeError(LEVEL + " exists; set TH_REPLACE=1 to re-import")

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

manager = unreal.InterchangeManager.get_interchange_manager_scripted()
source = unreal.InterchangeManager.create_source_data(FBX)
params = unreal.ImportAssetParameters()
params.is_automated = True
ok = manager.import_scene(ASSETS, source, params)
out("import_scene returned %s" % ok)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
meshes = [a for a in actors if isinstance(a, unreal.StaticMeshActor)]
out("level has %d actors, %d static mesh actors" % (len(actors), len(meshes)))
if meshes:
    lo = None
    hi = None
    for a in meshes:
        origin, extent = a.get_actor_bounds(False)
        mn, mx = origin - extent, origin + extent
        lo = mn if lo is None else unreal.Vector(min(lo.x, mn.x), min(lo.y, mn.y), min(lo.z, mn.z))
        hi = mx if hi is None else unreal.Vector(max(hi.x, mx.x), max(hi.y, mx.y), max(hi.z, mx.z))
    out("bounds min %s max %s (cm)" % (lo, hi))

levels.save_current_level()
saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
out("saved level and assets: %s" % saved)
assets = eal.list_assets(ASSETS, recursive=True)
kinds = {}
for p in assets:
    data = eal.find_asset_data(p)
    kind = str(data.asset_class_path.asset_name)
    kinds[kind] = kinds.get(kind, 0) + 1
out("assets in %s: %s" % (ASSETS, kinds))

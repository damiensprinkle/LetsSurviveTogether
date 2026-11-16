"""Builds /Game/Maps/L_TestFacility: a generic test facility for playing against every kind of enemy, the director and
light and dark (developer, 2026-11-13: a generic test facility is fine; map design comes later). The firing range kit's
pieces, like L_EnemyTest (that map stays for the automated tests, which rely on its hall). One cell per 4 m tile:

        x  0 1 2 3 4 5 6 7 8 9
     y 0   S S c O O c W W W W     S  start room, lit: the players start here, a flashlight to pick up
       1   S S c O O c W W W W     O  offices, dark: crates for cover, loose boxes, the storage key; the stalker
       2   c c c c c c c c c c     W  warehouse 16 x 8 m, lit at its far end: loose boxes; the charger
       3   L L c . K K . B B B     c  corridors, one light in the middle: a hunter paces; loops all round
       4   L L c . K K . B B B     L  lab, lit: the siren.  K  storage, dark, LOCKED (the key is in the offices): two
                                       ambushers playing dead.  B  break room, dark: an ambusher pack playing dead

The director (DA_Director) brings enemies in at four spawn points, corridor ends out of the way.

Doors are the template's own (BP_Door, opening as a player or an enemy comes; BP_DoorLock needs BP_Key): unlocked ones
don't block the navigation mesh (enemies walk up and open them), the locked one does. Loose boxes simulate physics (a
charger knocks them aside). Walls 4.5 m with a ceiling, exposure pinned so unlit rooms stay dark.

Re-running replaces the whole level. The first time it needs the full editor (making a level); after that headless:
  UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>/Scripts/Tools/build_test_facility.py" -RenderOffScreen -unattended -nosound
  UnrealEditor-Cmd.exe "<abs>\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/build_test_facility.py" -nullrhi -unattended
"""
import math
import unreal

LEVEL = "/Game/Maps/L_TestFacility"
MESHES = "/Game/ThirdParty/FiringRange/Meshes/"
TILE = 400.4    # SM_FireRange_FloorPlane01
WALL = 200.2    # SM_FireRange_Wall01 / DoorwayWall01
FLOOR_Z = 0.0
HEIGHT = 450.4  # the kit's wall height
INSET = 4.0     # walls stand this far inside their cell, so two cells' walls are back to back
EXPOSURE = 1.0

LAYOUT = [
    "SScOOcWWWW",
    "SScOOcWWWW",
    "cccccccccc",
    "LLc.KK.BBB",
    "LLc.KK.BBB",
]
# Openings between cells of different areas: ((x, y), (x, y), what fills it: open / door / locked)
DOOR_LIST = [
    ((1, 1), (2, 1), "open"),     # start -> east corridor
    ((0, 1), (0, 2), "door"),     # start -> long corridor
    ((2, 0), (3, 0), "door"),     # corridor -> offices (west)
    ((4, 1), (5, 1), "open"),     # offices -> corridor (east)
    ((5, 0), (6, 0), "door"),     # corridor -> warehouse
    ((9, 1), (9, 2), "open"),     # warehouse -> long corridor
    ((1, 3), (1, 2), "door"),     # lab -> long corridor
    ((1, 4), (2, 4), "open"),     # lab -> side corridor
    ((4, 2), (4, 3), "locked"),   # long corridor -> storage (key in the offices)
    ((8, 2), (8, 3), "door"),     # long corridor -> break room
]
DOORS = [(a, b) for a, b, _ in DOOR_LIST]
DOOR_BP = "/Game/Template/Interaction/Blueprint/Door/BP_Door"
LOCKED_BP = "/Game/Template/Interaction/Blueprint/Door/BP_DoorLock"
KEY_BP = "/Game/Template/Interaction/Blueprint/Door/BP_Key"
KEY_SPOT = (4.4, 0.3)
FLASHLIGHT_BP = "/Game/Template/Interaction/Blueprint/FlashLight/BP_FlashLight"
FLASHLIGHT_SPOT = (0.8, 0.7)
# Lit: the start room, the warehouse's far end, the lab, the middle of the long corridor. Offices, storage and the break
# room are dark.
LIGHTS = [(0.5, 0.5), (8.5, 0.5), (0.5, 3.5), (4.5, 2.0)]
CRATES = [(3.3, 0.4), (4.2, 1.3), (7.0, 0.3), (8.0, 1.4)]
# Loose boxes (physics): (cell x, cell y, size cm)
PROPS = [(6.6, 1.0, 60), (7.5, 0.8, 50), (7.2, 1.5, 70), (8.6, 1.2, 60), (3.7, 0.9, 50), (6.5, 2.1, 55)]
PLAYER_STARTS = [(0.2, 0.0), (0.2, 0.6), (0.6, 0.3), (0.6, 0.9)]
BEHAVIORS = "/Game/Enemies/Behaviors/"
ZOMBIE_BP = "/Game/Enemies/BP_Zombie"
CAPSULE_HALF_HEIGHT = 90.0
# (cell x, cell y, behaviour, facing cell x, facing cell y)
ENEMIES = [
    (3.5, 0.6, "DA_Stalker", 2.0, 1.0),
    (8.8, 0.8, "DA_Charger", 6.0, 0.5),
    (0.4, 3.8, "DA_Siren", 1.0, 3.0),
    (6.5, 2.0, "DA_Hunter", 4.0, 2.0),
    (4.3, 3.5, "DA_Ambusher", 4.0, 2.0), (5.3, 4.2, "DA_Ambusher", 4.0, 2.0),
    (7.5, 3.4, "DA_Ambusher", 8.0, 2.0), (8.5, 3.7, "DA_Ambusher", 8.0, 2.0), (9.2, 4.3, "DA_Ambusher", 8.0, 2.0),
    (7.8, 4.5, "DA_Ambusher", 8.0, 2.0),
]
ENEMY_FOLDER = "TH_Enemies"
DIRECTOR_SETTINGS = "/Game/Enemies/Director/DA_Director"
# Where the director may bring enemies in (cell x, cell y): corridor ends and corners, out of the way
SPAWN_POINTS = [(2.0, 4.3), (5.0, 0.0), (9.7, 2.0), (0.0, 2.0)]

FACE_DOWN = unreal.Rotator(roll=-90, pitch=0, yaw=-90)
FACE_UP = unreal.Rotator(roll=90, pitch=0, yaw=-90)
WALL_FACING = {
    "+X": unreal.Rotator(roll=90, pitch=0, yaw=-90),
    "-X": unreal.Rotator(roll=90, pitch=0, yaw=90),
    "+Y": unreal.Rotator(roll=90, pitch=0, yaw=0),
    "-Y": unreal.Rotator(roll=90, pitch=0, yaw=180),
}

eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def out(msg):
    unreal.log_warning("[TH Facility] " + msg)


def spawn_mesh(mesh, location, rotation):
    """A static mesh actor (spawned from the class and given the mesh: spawning from the asset goes through the editor's
    placement code, which crashes headless)."""
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    actor.static_mesh_component.set_static_mesh(mesh)
    return actor


def world(cx, cy, z=FLOOR_Z):
    return unreal.Vector(cx * TILE, cy * TILE, z)


def piece(mesh_name, label, rotation, centre):
    """Places a kit mesh so its bounds are centred on `centre` (the kit's pivots are at corners and edges)."""
    actor = spawn_mesh(eal.load_asset(MESHES + mesh_name), unreal.Vector(0, 0, 0), rotation)
    origin, _ = actor.get_actor_bounds(False)
    actor.set_actor_location(centre - origin, False, False)
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    actor.set_actor_label(label)
    return actor


CUBE = "/Engine/BasicShapes/Cube"  # 100 cm, with simple box collision
blockers = 0


def blocker(label, centre, size, rotation=None):
    """An invisible box with simple collision. The kit's pieces only have complex (per-triangle) collision, which the
    navigation mesh doesn't see, so each floor, wall and crate gets one of these where it is."""
    global blockers
    actor = spawn_mesh(eal.load_asset(CUBE), centre, rotation or unreal.Rotator(0, 0, 0))
    actor.set_actor_scale3d(unreal.Vector(size.x / 100.0, size.y / 100.0, size.z / 100.0))
    component = actor.static_mesh_component
    component.set_visibility(False)
    component.set_collision_profile_name("BlockAll")
    actor.set_actor_label("Nav_" + label)
    actor.set_folder_path("NavBlockers")
    blockers += 1
    return actor


def doorway_opening():
    """Where the kit's doorway piece is open, along it (from its middle) and how high: traced on a spare copy."""
    probe = piece("SM_FireRange_DoorwayWall01", "Probe", WALL_FACING["-X"], unreal.Vector(0, 50000, HEIGHT / 2))
    world_ = unreal.EditorLevelLibrary.get_editor_world()
    open_at = []
    for i in range(-100, 101, 2):
        hit = unreal.SystemLibrary.line_trace_single(world_, unreal.Vector(-50, 50000 + i, 100), unreal.Vector(50, 50000 + i, 100),
                                                      unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        if hit is None or not hit.to_tuple()[0]:
            open_at.append(i)
    top = 0
    middle = (min(open_at) + max(open_at)) / 2
    for z in range(100, int(HEIGHT), 2):
        hit = unreal.SystemLibrary.line_trace_single(world_, unreal.Vector(-50, 50000 + middle, z), unreal.Vector(50, 50000 + middle, z),
                                                      unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        if hit is not None and hit.to_tuple()[0]:
            top = z
            break
    actors.destroy_actor(probe)
    out("doorway opening: %.0f..%.0f cm along the piece, %.0f cm high" % (min(open_at), max(open_at), top))
    return min(open_at), max(open_at), top


cells = {(x, y): ch for y, row in enumerate(LAYOUT) for x, ch in enumerate(row) if ch != "."}
doors = set()
for a, b in DOORS:
    doors.add((a, b))
    doors.add((b, a))

if eal.does_asset_exist(LEVEL):
    # Rebuilt in place: works headless too (making a new level needs the full editor)
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    # Only the pieces this script places (meshes, lights, starts, enemies left by setup_enemies.py); the exposure volume,
    # navigation bounds and navigation mesh are kept and reused below (destroying those headless crashes the engine)
    for actor in actors.get_all_level_actors():
        if (isinstance(actor, (unreal.StaticMeshActor, unreal.PlayerStart)) or 'OverheadLight' in actor.get_class().get_name()
                or str(actor.get_folder_path()).startswith('TH_')):
            actors.destroy_actor(actor)
else:
    unreal.EditorLevelLibrary.new_level(LEVEL)

OPEN_FROM, OPEN_TO, OPEN_TOP = doorway_opening()

# Floors and ceilings
for (x, y) in cells:
    piece("SM_FireRange_FloorPlane01", "Floor_%d_%d" % (x, y), FACE_UP, world(x, y))
    piece("SM_FireRange_FloorPlane01", "Ceiling_%d_%d" % (x, y), FACE_DOWN, world(x, y, FLOOR_Z + HEIGHT))
    blocker("Floor_%d_%d" % (x, y), world(x, y, FLOOR_Z - 5.0), unreal.Vector(TILE, TILE, 10.0))

# Walls: on every cell edge that isn't open to the same area, facing into the cell. Each edge is two wall pieces; a
# door replaces the first with the doorway piece.
SIDES = [((1, 0), "-X"), ((-1, 0), "+X"), ((0, 1), "-Y"), ((0, -1), "+Y")]
walls = openings = 0
for (x, y), area in cells.items():
    for (dx, dy), facing in SIDES:
        neighbour = (x + dx, y + dy)
        if cells.get(neighbour) == area:
            continue  # the same room or corridor: open
        is_door = ((x, y), neighbour) in doors
        # The edge's middle, pulled back into this cell; the two pieces sit either side of it along the edge
        edge = world(x + dx * 0.5, y + dy * 0.5, FLOOR_Z + HEIGHT / 2)
        edge = unreal.Vector(edge.x - dx * INSET, edge.y - dy * INSET, edge.z)
        along = unreal.Vector(abs(dy), abs(dx), 0)
        for k, offset in enumerate((-WALL / 2, WALL / 2)):
            mesh = "SM_FireRange_DoorwayWall01" if is_door and k == 0 else "SM_FireRange_Wall01"
            label = "Wall_%d_%d_%s%d" % (x, y, facing, k)
            centre = unreal.Vector(edge.x + along.x * offset, edge.y + along.y * offset, edge.z)
            doorway = piece(mesh, label, WALL_FACING[facing], centre)
            walls += 1

            # Its blocker, 10 cm thick behind the visible face; a doorway's leaves the opening clear. The opening was
            # measured along the piece's world Y when facing -X; the facings here turn it, so measure from the piece.
            def wall_box(name, start, end, bottom, top):
                middle = (start + end) / 2
                size_along = end - start
                box_centre = unreal.Vector(centre.x + along.x * middle + dx * 5.0, centre.y + along.y * middle + dy * 5.0, FLOOR_Z + (bottom + top) / 2)
                blocker(name, box_centre, unreal.Vector(size_along if along.x else 10.0, size_along if along.y else 10.0, top - bottom))
            if mesh == "SM_FireRange_Wall01":
                wall_box(label, -WALL / 2, WALL / 2, 0.0, HEIGHT)
            else:
                # Where the opening is on this copy: trace along it at knee height
                world_ = unreal.EditorLevelLibrary.get_editor_world()
                gaps = []
                for i in range(-100, 101, 2):
                    p = unreal.Vector(centre.x + along.x * i, centre.y + along.y * i, FLOOR_Z + 100)
                    hit = unreal.SystemLibrary.line_trace_single(world_, unreal.Vector(p.x - dx * 30, p.y - dy * 30, p.z), unreal.Vector(p.x + dx * 30, p.y + dy * 30, p.z),
                                                                  unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
                    if hit is None or not hit.to_tuple()[0]:
                        gaps.append(i)
                lo, hi = min(gaps), max(gaps)
                wall_box(label + "_A", -WALL / 2, lo - 1.0, 0.0, HEIGHT)
                wall_box(label + "_B", hi + 1.0, WALL / 2, 0.0, HEIGHT)
                wall_box(label + "_Top", lo - 1.0, hi + 1.0, OPEN_TOP, HEIGHT)
        openings += 1 if is_door else 0
out("%d cells, %d wall pieces, %d doorway sides, %d navigation blockers so far" % (len(cells), walls, openings, blockers))

world_ = unreal.EditorLevelLibrary.get_editor_world()


def blocked(start, end):
    hit = unreal.SystemLibrary.line_trace_single(world_, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                  unreal.DrawDebugTrace.NONE, True)
    return hit is not None and hit.to_tuple()[0]


def mesh_bounds(actor):
    """Bounds of the actor's visible meshes only (a door's trigger volumes would make it look bigger)."""
    lo = unreal.Vector(1e9, 1e9, 1e9)
    hi = unreal.Vector(-1e9, -1e9, -1e9)
    for component in list(actor.get_components_by_class(unreal.StaticMeshComponent)) + list(actor.get_components_by_class(unreal.SkeletalMeshComponent)):
        if not component.is_visible():
            continue
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(component)
        lo = unreal.Vector(min(lo.x, origin.x - extent.x), min(lo.y, origin.y - extent.y), min(lo.z, origin.z - extent.z))
        hi = unreal.Vector(max(hi.x, origin.x + extent.x), max(hi.y, origin.y + extent.y), max(hi.z, origin.z + extent.z))
    return (lo + hi) * 0.5, (hi - lo) * 0.5


# Doors in their doorways: centred on the opening (found by tracing across the edge), the long side along the wall,
# standing on the floor. Unlocked ones don't block the navigation mesh (enemies walk up and open them)
door_class = eal.load_blueprint_class(DOOR_BP)
locked_class = eal.load_blueprint_class(LOCKED_BP)
for (ax, ay), (bx, by), kind in DOOR_LIST:
    if kind == "open":
        continue
    dx, dy = bx - ax, by - ay
    edge = world(ax + dx * 0.5, ay + dy * 0.5, FLOOR_Z + 100.0)
    along = unreal.Vector(abs(dy), abs(dx), 0)
    gaps = [i for i in range(-200, 201, 4)
            if not blocked(unreal.Vector(edge.x + along.x * i - dx * 60, edge.y + along.y * i - dy * 60, edge.z),
                           unreal.Vector(edge.x + along.x * i + dx * 60, edge.y + along.y * i + dy * 60, edge.z))]
    if not gaps:
        out("door %s-%s: no opening found" % ((ax, ay), (bx, by)))
        continue
    middle = (min(gaps) + max(gaps)) / 2.0
    centre = unreal.Vector(edge.x + along.x * middle, edge.y + along.y * middle, FLOOR_Z)
    door = actors.spawn_actor_from_class(locked_class if kind == "locked" else door_class, centre, unreal.Rotator(0, 0, 0))
    origin, extent = mesh_bounds(door)
    if (extent.x > extent.y) != (along.x > 0):
        door.set_actor_rotation(unreal.Rotator(0, 0, 90), False)
        origin, extent = mesh_bounds(door)
    location = door.get_actor_location()
    door.set_actor_location(unreal.Vector(location.x + centre.x - origin.x, location.y + centre.y - origin.y,
                                          location.z + FLOOR_Z - (origin.z - extent.z)), False, False)
    door.set_actor_label("Door_%d_%d_%s" % (ax, ay, kind))
    door.set_folder_path("TH_Doors")
    if kind == "door":
        for component in door.get_components_by_class(unreal.PrimitiveComponent):
            component.set_editor_property("can_ever_affect_navigation", False)
    out("door %s-%s (%s): %.0f cm wide, %.0f high, in an opening %.0f wide, %.0f high"
        % ((ax, ay), (bx, by), kind, 2 * max(extent.x, extent.y), 2 * extent.z, max(gaps) - min(gaps), OPEN_TOP))

# Lights, cover, loose boxes, player starts, the key and a flashlight
light_class = eal.load_blueprint_class("/Game/Environment/BP_OverheadLight")
for i, (lx, ly) in enumerate(LIGHTS):
    actors.spawn_actor_from_class(light_class, world(lx, ly, FLOOR_Z + HEIGHT), unreal.Rotator(0, 0, 0)).set_actor_label("Light%d" % (i + 1))
crate = eal.load_asset(MESHES + "SM_Crate")
for i, (cx, cy) in enumerate(CRATES):
    actor = spawn_mesh(crate, world(cx, cy), unreal.Rotator(0, 0, 37.0 * i))
    origin, extent = actor.get_actor_bounds(False)
    location = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(location.x, location.y, location.z + FLOOR_Z - (origin.z - extent.z)), False, False)
    actor.set_actor_label("Crate%d" % (i + 1))
    origin, _ = actor.get_actor_bounds(False)
    box = crate.get_bounding_box()
    scale = actor.get_actor_scale3d()
    blocker("Crate%d" % (i + 1), origin, unreal.Vector((box.max.x - box.min.x) * scale.x, (box.max.y - box.min.y) * scale.y, (box.max.z - box.min.z) * scale.z),
            actor.get_actor_rotation())
crate_material = crate.get_material(0)
for i, (px, py, size) in enumerate(PROPS):
    # Engine cubes (simple collision, so they can simulate) in the crate material
    box = spawn_mesh(eal.load_asset(CUBE), world(px, py, FLOOR_Z + size / 2.0 + 1.0), unreal.Rotator(0, 0, 23.0 * i))
    box.set_actor_scale3d(unreal.Vector(size / 100.0, size / 100.0, size / 100.0))
    component = box.static_mesh_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_material(0, crate_material)
    component.set_collision_profile_name("PhysicsActor")
    component.set_simulate_physics(True)
    component.set_editor_property("can_ever_affect_navigation", False)
    box.set_actor_label("LooseBox%d" % (i + 1))
    box.set_folder_path("TH_Props")
for i, (px, py) in enumerate(PLAYER_STARTS):
    actors.spawn_actor_from_class(unreal.PlayerStart, world(px, py, FLOOR_Z + 100.0), unreal.Rotator(0, 0, 0)).set_actor_label("PlayerStart%d" % (i + 1))
key = actors.spawn_actor_from_class(eal.load_blueprint_class(KEY_BP), world(KEY_SPOT[0], KEY_SPOT[1], FLOOR_Z + 80.0), unreal.Rotator(0, 0, 0))
key.set_actor_label("StorageKey")
key.set_folder_path("TH_Pickups")
flashlight = actors.spawn_actor_from_class(eal.load_blueprint_class(FLASHLIGHT_BP), world(FLASHLIGHT_SPOT[0], FLASHLIGHT_SPOT[1], FLOOR_Z + 10.0), unreal.Rotator(0, 0, 90))
flashlight.set_actor_label("FlashLight")
flashlight.set_folder_path("TH_Pickups")

# The enemies, each kind where it belongs
zombie_class = eal.load_blueprint_class(ZOMBIE_BP)
for i, (cx, cy, behavior, fx, fy) in enumerate(ENEMIES):
    yaw = math.degrees(math.atan2(fy - cy, fx - cx))
    zombie = actors.spawn_actor_from_class(zombie_class, world(cx, cy, FLOOR_Z + CAPSULE_HALF_HEIGHT + 2.0), unreal.Rotator(0, 0, yaw))
    zombie.set_actor_label("Enemy%d_%s" % (i + 1, behavior[3:]))
    zombie.set_folder_path(ENEMY_FOLDER)
    zombie.set_editor_property("behavior", eal.load_asset(BEHAVIORS + behavior))
out("%d enemies, %d loose boxes, %d lights" % (len(ENEMIES), len(PROPS), len(LIGHTS)))

# The director (DA_Director, setup_director.py) and where it may bring enemies in: the far ends of corridors, dark
director = actors.spawn_actor_from_class(unreal.THDirector, world(0.5, 0.5, FLOOR_Z + 300.0), unreal.Rotator(0, 0, 0))
director.set_editor_property("settings", eal.load_asset(DIRECTOR_SETTINGS))
director.set_actor_label("Director")
director.set_folder_path("TH_Director")
for i, (sx, sy) in enumerate(SPAWN_POINTS):
    point = actors.spawn_actor_from_class(unreal.THSpawnPoint, world(sx, sy, FLOOR_Z + 5.0), unreal.Rotator(0, 0, 0))
    point.set_actor_label("SpawnPoint%d" % (i + 1))
    point.set_folder_path("TH_Director")
out("director with %d spawn points" % len(SPAWN_POINTS))

# Exposure pinned, like the range
existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PostProcessVolume)]
volume = existing[0] if existing else actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
volume.set_actor_label("Exposure")
volume.set_editor_property("unbound", True)
settings = volume.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_min_brightness", True)
settings.set_editor_property("override_auto_exposure_max_brightness", True)
settings.set_editor_property("auto_exposure_min_brightness", EXPOSURE)
settings.set_editor_property("auto_exposure_max_brightness", EXPOSURE)
volume.set_editor_property("settings", settings)

# Navigation: bounds over the whole layout, built here and saved with the level (static)
width = max(x for x, _ in cells) + 1
depth = max(y for _, y in cells) + 1
existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)]
bounds = existing[0] if existing else actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, world((width - 1) / 2, (depth - 1) / 2, FLOOR_Z + HEIGHT / 2))
bounds.set_actor_location(world((width - 1) / 2, (depth - 1) / 2, FLOOR_Z + HEIGHT / 2), False, False)
if not unreal.THEditorScriptLibrary.build_box_brush(bounds, unreal.Vector(width * TILE + 200.0, depth * TILE + 200.0, HEIGHT + 100.0)):
    raise RuntimeError("Could not shape the navigation bounds")
bounds.set_actor_label("NavBounds")
existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.RecastNavMesh)]
navmesh = existing[0] if existing else actors.spawn_actor_from_class(unreal.RecastNavMesh, unreal.Vector(0, 0, 0))
navmesh.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.STATIC)
seeds = [world(x + 0.0, y + 0.0, FLOOR_Z + 50) for (x, y) in sorted(cells)]
out("navigation: " + unreal.THEditorScriptLibrary.build_navigation(world_, seeds))
unreal.EditorLoadingAndSavingUtils.save_map(world_, LEVEL)
out("saved %s" % LEVEL)
unreal.SystemLibrary.quit_editor()

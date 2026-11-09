"""Builds /Game/Maps/L_EnemyTest: a small dark level for the first enemies (Milestone 4), from the firing range kit's
wall and floor pieces. Layout, one cell per 4 m floor tile (x to the right, y down):

        x  0 1 2 3 4 5 6
     y 0   S S . H H H H     S  start room 8 x 8 m, lit; the players start here, facing the hall
       1   S S c H H H H     H  hall 16 x 12 m, one light in its far corner, crates for cover
       2   . c . H H H H     R  side room 8 x 8 m, dark
       3   . c . . . c .     c  corridors 4 m wide, dark
       4   R R c c c c .
       5   R R . . . . .

Two ways from the start room to the hall (east, or south through the side room and back up), so there is a loop to
patrol and flank. Doorways are the kit's doorway wall piece with no door. Walls 4.5 m with a ceiling, exposure pinned
like the range so unlit places stay dark. The navigation mesh is built by this script and saved with the level (static). The kit pieces only have complex
collision, which navigation ignores, so every floor, wall and crate also gets an invisible box (folder NavBlockers).
After moving things in the editor, rebuild navigation there (Build, Build Paths) or re-run this script.

Re-running replaces the whole level. Not headless (creating a level needs the full editor); editor closed:
  UnrealEditor.exe "<abs>\\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>/Scripts/Tools/build_enemy_test_map.py" -RenderOffScreen -unattended -nosound
"""
import unreal

LEVEL = "/Game/Maps/L_EnemyTest"
MESHES = "/Game/ThirdParty/FiringRange/Meshes/"
TILE = 400.4    # SM_FireRange_FloorPlane01
WALL = 200.2    # SM_FireRange_Wall01 / DoorwayWall01
FLOOR_Z = 0.0
HEIGHT = 450.4  # the kit's wall height
INSET = 4.0     # walls stand this far inside their cell, so two cells' walls are back to back
EXPOSURE = 1.0

LAYOUT = [
    "SS.HHHH",
    "SScHHHH",
    ".c.HHHH",
    ".c...c.",
    "RRcccc.",
    "RR.....",
]
# Openings between cells of different areas: ((x, y), (x, y))
DOORS = [
    ((1, 1), (2, 1)), ((2, 1), (3, 1)),   # start room -> east corridor -> hall
    ((1, 1), (1, 2)), ((1, 3), (1, 4)),   # start room -> south corridor -> side room
    ((1, 4), (2, 4)), ((5, 3), (5, 2)),   # side room -> long corridor -> hall
]
LIGHTS = [(0.5, 0.5), (6.0, 0.0)]  # start room's middle; the hall's far corner (cell coordinates)
CRATES = [(4.0, 0.6), (4.3, 1.7), (5.6, 1.2), (3.6, 2.3)]
PLAYER_STARTS = [(0.2, 0.0), (0.2, 0.6), (0.2, 1.2), (0.6, 0.3)]

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
    unreal.log_warning("[TH EnemyTest] " + msg)


def world(cx, cy, z=FLOOR_Z):
    return unreal.Vector(cx * TILE, cy * TILE, z)


def piece(mesh_name, label, rotation, centre):
    """Places a kit mesh so its bounds are centred on `centre` (the kit's pivots are at corners and edges)."""
    actor = actors.spawn_actor_from_object(eal.load_asset(MESHES + mesh_name), unreal.Vector(0, 0, 0), rotation)
    origin, _ = actor.get_actor_bounds(False)
    actor.set_actor_location(centre - origin, False, False)
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    actor.set_actor_label(label)
    return actor


CUBE = "/Engine/BasicShapes/Cube"  # 100 cm, with simple box collision
blockers = 0


def blocker(label, centre, size):
    """An invisible box with simple collision. The kit's pieces only have complex (per-triangle) collision, which the
    navigation mesh doesn't see, so each floor, wall and crate gets one of these where it is."""
    global blockers
    actor = actors.spawn_actor_from_object(eal.load_asset(CUBE), centre, unreal.Rotator(0, 0, 0))
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
    eal.delete_asset(LEVEL)
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

# Lights, cover, player starts
light_class = eal.load_blueprint_class("/Game/Environment/BP_OverheadLight")
for i, (lx, ly) in enumerate(LIGHTS):
    actors.spawn_actor_from_class(light_class, world(lx, ly, FLOOR_Z + HEIGHT), unreal.Rotator(0, 0, 0)).set_actor_label("Light%d" % (i + 1))
crate = eal.load_asset(MESHES + "SM_Crate")
for i, (cx, cy) in enumerate(CRATES):
    actor = actors.spawn_actor_from_object(crate, world(cx, cy), unreal.Rotator(0, 0, 37.0 * i))
    origin, extent = actor.get_actor_bounds(False)
    location = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(location.x, location.y, location.z + FLOOR_Z - (origin.z - extent.z)), False, False)
    actor.set_actor_label("Crate%d" % (i + 1))
    origin, extent = actor.get_actor_bounds(False)
    blocker("Crate%d" % (i + 1), origin, unreal.Vector(extent.x * 2, extent.y * 2, extent.z * 2))
for i, (px, py) in enumerate(PLAYER_STARTS):
    actors.spawn_actor_from_class(unreal.PlayerStart, world(px, py, FLOOR_Z + 100.0), unreal.Rotator(0, 0, 0)).set_actor_label("PlayerStart%d" % (i + 1))

# Exposure pinned, like the range's
volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
volume.set_actor_label("Exposure")
volume.set_editor_property("unbound", True)
settings = volume.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_min_brightness", True)
settings.set_editor_property("override_auto_exposure_max_brightness", True)
settings.set_editor_property("auto_exposure_min_brightness", EXPOSURE)
settings.set_editor_property("auto_exposure_max_brightness", EXPOSURE)
volume.set_editor_property("settings", settings)

# Navigation: bounds over the whole layout, generated at runtime
width = max(x for x, _ in cells) + 1
depth = max(y for _, y in cells) + 1
bounds = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, world((width - 1) / 2, (depth - 1) / 2, FLOOR_Z + HEIGHT / 2))
# A volume spawned from Python has no shape until a brush builder gives it one (Python can't reach the builders)
if not unreal.THEditorScriptLibrary.build_box_brush(bounds, unreal.Vector(width * TILE + 200.0, depth * TILE + 200.0, HEIGHT + 100.0)):
    raise RuntimeError("Could not shape the navigation bounds")
bounds.set_actor_label("NavBounds")
origin, extent = bounds.get_actor_bounds(False)
out("navigation bounds %s +- %s" % (origin, extent))
navmesh = actors.spawn_actor_from_class(unreal.RecastNavMesh, unreal.Vector(0, 0, 0))
# Static: built here (build_navigation, below) and saved with the level. Nothing in it moves. A full rebuild while
# playing takes over 20 s, so don't rebuild on load. Switch to Dynamic if doors or moving obstacles are added.
navmesh.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.STATIC)
# Its 10 cm voxels come from Config/DefaultEngine.ini (RecastNavMesh): with the default 19 cm the doorways close

out("navigation: " + unreal.THEditorScriptLibrary.build_navigation(unreal.EditorLevelLibrary.get_editor_world(),
    [world(0.5, 0.5, FLOOR_Z + 50), world(1.5, 0.75, FLOOR_Z + 50), world(1.5, 1.0, FLOOR_Z + 50), world(1.75, 1.0, FLOOR_Z + 50), world(4.5, 1.0, FLOOR_Z + 50), world(0.5, 4.5, FLOOR_Z + 50)]))
levels.save_current_level()
out("saved %s" % LEVEL)
unreal.SystemLibrary.quit_editor()

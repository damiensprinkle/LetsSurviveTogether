"""Finishes the shell of /Game/Maps/L_FiringRange: a ceiling, the template door in the kit's doorway, a room behind it.

The Fab scene has no ceiling (above the walls is black void, which also drives auto exposure far too bright). This adds:
  Ceiling   kit floor plates, face down: Z 334 over the room behind the counters, Z 531 downrange, and a header closing
            the step between them. Over 400 cm tiles so the kit's texture keeps its scale.
  Door      the template's BP_Door (interactable) in SM_FireRange_DoorwayWall01's opening, replacing the static SM_ExitDoor.
            Its frame is 212 cm tall and the opening 232, hence Z scale 1.095.
  Room      an empty 16 x 16 m room behind the door (the developer's size, 2026-11-09), made of the kit's wall and floor
            pieces, with four overhead lights. The players start here (TH_PlayerStart1..4, facing the door).
  Exposure  an unbound PostProcessVolume pinning exposure like the one in Demo, so light intensities mean something.

Everything it adds is labelled TH_ and in the World Outliner folder TH_RangeShell: re-running replaces those and nothing else
(other TH_ folders, e.g. setup_target_carriers.py's, are left alone). It also removes the level's original PlayerStart.
Not headless (the level holds Blueprint actors and a save). Editor closed:
  UnrealEditor.exe "<abs>\\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\\Scripts\\Tools\\build_range_room.py" -RenderOffScreen -unattended -nosound
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Maps/L_FiringRange"
MESHES = "/Game/ThirdParty/FiringRange/Meshes/"
FOLDER = "TH_RangeShell"
FLOOR_Z = -116.0
ROOM_CEILING_Z = 334.0   # top of the kit's 450 cm walls (and of its corner ceiling plates)
RANGE_CEILING_Z = 531.0  # top of the downrange walls
STEP_Y = -873.0          # where the ceiling steps up (downrange face of the kit's ceiling edge pieces)
STEP_BOTTOM_Z = 427.0    # top of those pieces
TILE = 400.4             # SM_FireRange_FloorPlane01
WALL = 200.2             # SM_FireRange_Wall01 / DoorwayWall01
DOOR_LOCATION = unreal.Vector(-391.0, -193.0, FLOOR_Z)  # centre of the opening
DOOR_SCALE = unreal.Vector(1.0, 1.0, 1.095)
# The room: X from the door wall to the west, Y clear of the range's thick side wall (it ends at -443)
ROOM_EAST = -390.0
ROOM_WEST = ROOM_EAST - 4 * TILE
ROOM_SOUTH = -440.0
DOOR_Y_START = 60.0  # the player starts' middle, clear of the door's swing
ROOM_NORTH = ROOM_SOUTH + 4 * TILE
FIXTURES_UNDER_ROOM_CEILING = ("BP_OverheadLight", "BP_OverheadLight2")
# The fixtures' origin is their hanging point: a 2 x 2 grid, each over the middle of a quarter of the room
ROOM_LIGHTS = [unreal.Vector(ROOM_WEST + (ROOM_EAST - ROOM_WEST) * fx, ROOM_SOUTH + (ROOM_NORTH - ROOM_SOUTH) * fy, ROOM_CEILING_Z)
               for fx in (0.25, 0.75) for fy in (0.25, 0.75)]
# Where the players start: in the room, a line across it 3 m in from the door, facing it (+X). Z is the capsule's centre.
PLAYER_STARTS = [unreal.Vector(ROOM_EAST - 300.0, DOOR_Y_START + dy, FLOOR_Z + 100.0) for dy in (-150.0, 0.0, 150.0, 300.0)]
EXPOSURE = 1.0  # same as Demo's PostProcessVolume

# The kit's plates lie in local XZ with their face along local +Y, so roll says where they face
FACE_DOWN = unreal.Rotator(roll=-90, pitch=0, yaw=-90)
FACE_UP = unreal.Rotator(roll=90, pitch=0, yaw=-90)
FACE_DOWNRANGE = unreal.Rotator(roll=0, pitch=0, yaw=180)
# The kit's walls, as the pack rotates them to face each way
WALL_FACING = {
    "+X": unreal.Rotator(roll=90, pitch=0, yaw=-90),
    "-X": unreal.Rotator(roll=90, pitch=0, yaw=90),
    "+Y": unreal.Rotator(roll=90, pitch=0, yaw=0),
    "-Y": unreal.Rotator(roll=90, pitch=0, yaw=180),
}


def out(msg):
    unreal.log_warning("[TH RangeRoom] " + msg)


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(LEVEL)

# Clear the earlier run, and the static door this replaces
for actor in actors.get_all_level_actors():
    label = actor.get_actor_label()
    if (label.startswith("TH_") and str(actor.get_folder_path()) == FOLDER) or label == "ExitDoor" or actor.get_class().get_name() == "PlayerStart":
        actors.destroy_actor(actor)


def piece(mesh_name, label, rotation, centre):
    """Places a kit mesh so its bounds are centred on `centre` (the kit's pivots are at corners and edges)."""
    actor = actors.spawn_actor_from_object(eal.load_asset(MESHES + mesh_name), unreal.Vector(0, 0, 0), rotation)
    origin, _ = actor.get_actor_bounds(False)
    actor.set_actor_location(centre - origin, False, False)
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    actor.set_actor_label("TH_" + label)
    actor.set_folder_path(FOLDER)
    return actor


def tiles(start, end, size):
    """Tile centres from start, enough to reach end (the last may run past it, hidden behind the walls)."""
    centres = []
    position = start + size / 2
    while position - size / 2 < end - 1:
        centres.append(position)
        position += size
    return centres


def ceiling(label, x0, x1, y0, y1, z):
    count = 0
    for x in tiles(x0, x1, TILE):
        for y in tiles(y0, y1, TILE):
            count += 1
            piece("SM_FireRange_FloorPlane01", "%s%02d" % (label, count), FACE_DOWN, unreal.Vector(x, y, z))
    out("%s: %d plates at Z %.0f" % (label, count, z))


# 1. Ceiling. The range's walls run X -416..1235 and it is 3800 deep. Plates past the walls' outer edges are hidden.
ceiling("CeilingRoom", -392.0, 1208.0, -473.0, -73.0, ROOM_CEILING_Z)
ceiling("CeilingStep", -420.0, 1240.0, STEP_Y, -473.0, STEP_BOTTOM_Z)
ceiling("CeilingRange", -420.0, 1240.0, -3800.0, STEP_Y, RANGE_CEILING_Z)
for i, x in enumerate(tiles(-420.0, 1240.0, TILE)):
    piece("SM_FireRange_FloorPlane01", "CeilingHeader%02d" % (i + 1), FACE_DOWNRANGE, unreal.Vector(x, STEP_Y, STEP_BOTTOM_Z + TILE / 2))

# 2. The door
door_class = eal.load_blueprint_class("/Game/Template/Interaction/Blueprint/Door/BP_Door")
door = actors.spawn_actor_from_class(door_class, DOOR_LOCATION, unreal.Rotator(0, 0, 0))
door.set_actor_scale3d(DOOR_SCALE)
door.set_actor_label("TH_RangeDoor")
door.set_folder_path(FOLDER)

# 3. The room
for i, x in enumerate(tiles(ROOM_WEST, ROOM_EAST, TILE)):
    for j, y in enumerate(tiles(ROOM_SOUTH, ROOM_NORTH, TILE)):
        piece("SM_FireRange_FloorPlane01", "RoomFloor%d%d" % (i, j), FACE_UP, unreal.Vector(x, y, FLOOR_Z))
ceiling("RoomCeiling", ROOM_WEST, ROOM_EAST, ROOM_SOUTH, ROOM_NORTH, ROOM_CEILING_Z)
WALL_Z = FLOOR_Z + 225.2
for i, x in enumerate(tiles(ROOM_WEST, ROOM_EAST, WALL)):
    piece("SM_FireRange_Wall01", "RoomWallS%d" % i, WALL_FACING["+Y"], unreal.Vector(x, ROOM_SOUTH, WALL_Z))
    piece("SM_FireRange_Wall01", "RoomWallN%d" % i, WALL_FACING["-Y"], unreal.Vector(x, ROOM_NORTH, WALL_Z))
for i, y in enumerate(tiles(ROOM_SOUTH, ROOM_NORTH, WALL)):
    piece("SM_FireRange_Wall01", "RoomWallW%d" % i, WALL_FACING["+X"], unreal.Vector(ROOM_WEST, y, WALL_Z))
# East wall: back to back with the range's wall (those pieces are one-sided), its doorway on the door. Tiled out from
# the opening; the ends run on past the room's corners, hidden behind its other walls.
EAST_X = ROOM_EAST - 9.0  # behind what the range hangs on its side of the wall (extinguisher, vent), inside the door frame
DOOR_Y = DOOR_LOCATION.y
piece("SM_FireRange_DoorwayWall01", "RoomWallDoorway", WALL_FACING["-X"], unreal.Vector(EAST_X, DOOR_Y, WALL_Z))
for i, y in enumerate([DOOR_Y - WALL] + [DOOR_Y + k * WALL for k in range(1, 9)]):
    piece("SM_FireRange_Wall01", "RoomWallE%d" % i, WALL_FACING["-X"], unreal.Vector(EAST_X, y, WALL_Z))

# 4. Lights: the two fixtures behind the counters hang from the new ceiling; one more for the room
light_class = eal.load_blueprint_class("/Game/Environment/BP_OverheadLight")
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() in FIXTURES_UNDER_ROOM_CEILING:
        location = actor.get_actor_location()
        actor.set_actor_location(unreal.Vector(location.x, location.y, ROOM_CEILING_Z), False, False)
        out("%s now hangs at Z %.0f" % (actor.get_actor_label(), ROOM_CEILING_Z))
for i, location in enumerate(ROOM_LIGHTS):
    room_light = actors.spawn_actor_from_class(light_class, location, unreal.Rotator(0, 0, 0))
    room_light.set_actor_label("TH_RoomLight%d" % (i + 1))
    room_light.set_folder_path(FOLDER)
for i, location in enumerate(PLAYER_STARTS):
    start = actors.spawn_actor_from_class(unreal.PlayerStart, location, unreal.Rotator(0, 0, 0))
    start.set_actor_label("TH_PlayerStart%d" % (i + 1))
    start.set_folder_path(FOLDER)

# 5. Exposure
volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
volume.set_actor_label("TH_Exposure")
volume.set_folder_path(FOLDER)
volume.set_editor_property("unbound", True)
settings = volume.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_min_brightness", True)
settings.set_editor_property("override_auto_exposure_max_brightness", True)
settings.set_editor_property("auto_exposure_min_brightness", EXPOSURE)
settings.set_editor_property("auto_exposure_max_brightness", EXPOSURE)
volume.set_editor_property("settings", settings)

levels.save_current_level()
out("saved %s" % LEVEL)
unreal.SystemLibrary.quit_editor()

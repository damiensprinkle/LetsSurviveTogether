"""Creates /Game/Tests/Maps/L_FunctionalTests, the map the functional automation tests run in (TESTING.md):
a large flat floor, three player starts and a light. The tests spawn whatever else they need. Re-run to rebuild it.

Run with the editor closed:
  UnrealEditor-Cmd LetsSurviveTogether.uproject -run=pythonscript -script=Scripts/Tools/create_test_map.py -unattended -nullrhi
"""
import unreal

MAP = "/Game/Tests/Maps/L_FunctionalTests"
CUBE = "/Game/Template/LevelPrototyping/Meshes/SM_Cube"

eal = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if eal.does_asset_exist(MAP):
    eal.delete_asset(MAP)
if not levels.new_level(MAP):
    raise RuntimeError("could not create " + MAP)

cube = eal.load_asset(CUBE)
bounds = cube.get_bounding_box()
size = bounds.max - bounds.min

# 100 m x 100 m floor, its top at Z = 0
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
floor.static_mesh_component.set_static_mesh(cube)
floor.set_actor_scale3d(unreal.Vector(10000.0 / size.x, 10000.0 / size.y, 100.0 / size.z))
# The cube's pivot is at a corner, not its centre: move the floor so its bounds are centred on the origin, top at Z = 0
origin, extent = floor.get_actor_bounds(False)
location = floor.get_actor_location()
floor.set_actor_location(unreal.Vector(location.x - origin.x, location.y - origin.y, location.z - (origin.z + extent.z)), False, False)
floor.set_actor_label("Floor")
origin, extent = floor.get_actor_bounds(False)
if abs(origin.x) > 1 or abs(origin.y) > 1 or extent.x < 4000 or extent.y < 4000:
    raise RuntimeError("floor not centred: origin %s extent %s" % (origin, extent))

# One start per player of a multiplayer test, side by side 2 m apart, all facing +X
for i, y in enumerate((0, -200, 200)):
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, y, 100))
    start.set_actor_label("PlayerStart%d" % (i + 1))

light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -50, 30))
light.set_actor_label("Sun")

levels.save_current_level()
unreal.log_warning("[TH TestMap] created %s (floor centre %s, half size %s)" % (MAP, origin, extent))

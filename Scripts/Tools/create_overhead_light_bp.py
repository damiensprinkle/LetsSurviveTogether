"""Creates /Game/Environment/BP_OverheadLight: the Fab overhead light fixture with its own light, so one drag places a
working ceiling light, and every copy shares the settings (edit them in the Blueprint).

  Fixture    SM_OverheadLight at the developer's scale (1.5, 1.75, 1), origin at the hanging point
  TubeLight  Point Light shaped like the tubes (Source Length = their length): shines in every direction like bare
             fluorescent tubes, so the ceiling, walls and floor around it are lit. (It was a downward Rect Light, which
             left the ceiling pitch black.)

Then, in /Game/Maps/L_FiringRange, every plain SM_OverheadLight the developer placed is swapped for the Blueprint at the
same place (its scale divided by the Blueprint's, so a fixture scaled 1.5 / 1.75 / 1 becomes a Blueprint at scale 1).

Not headless (creating a Blueprint needs Slate). Editor closed:
  UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\Scripts\Tools\create_overhead_light_bp.py" -RenderOffScreen -unattended -nosound
Safe to re-run: an existing Blueprint is updated in place (placed copies keep working) with the values below.
"""
import unreal

eal = unreal.EditorAssetLibrary
FOLDER = "/Game/Environment"
BP_PATH = FOLDER + "/BP_OverheadLight"
MESH = "/Game/ThirdParty/OverheadLight/Meshes/SM_OverheadLight"
LEVEL = "/Game/Maps/L_FiringRange"
SCALE = unreal.Vector(1.5, 1.75, 1.0)  # the developer's choice (2026-11-05)

# Mesh at scale 1: 12.9 (X) x 53.9 (Y) x 24.0 (Z) cm, hanging down from Z = 0; the tubes are at the bottom
TUBE_LENGTH = 53.9 * SCALE.y
# The two tubes run 23 cm below the hanging point (between Z -24 and -22 at scale 1)
TUBE_Z = -23.1 * SCALE.z
LIGHT = {
    "intensity_units": unreal.LightUnits.CANDELAS,
    "intensity": 100.0,
    "attenuation_radius": 1500.0,
    "source_radius": 2.0,
    "source_length": TUBE_LENGTH * 0.95,
    # The developer's choice: colour temperature 5000 K (2026-11-05), white tint
    "use_temperature": True,
    "temperature": 5000.0,
    "light_color": unreal.Color(r=255, g=255, b=255, a=255),
    "cast_shadows": True,
}


def out(msg):
    unreal.log_warning("[TH LightBP] " + msg)


sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def component_of(handle):
    return lib.get_object(lib.get_data(handle))


if eal.does_asset_exist(BP_PATH):
    # Update in place: deleting it would break the copies placed in levels
    bp = eal.load_asset(BP_PATH)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_OverheadLight", FOLDER, unreal.Blueprint, factory)
handles = sds.k2_gather_subobject_data_for_blueprint(bp)
root = handles[0]
existing = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}


def add(cls, name, parent):
    if name in existing:
        return existing[name]
    handle, failure = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp))
    if not str(failure) in ("", "None"):
        raise RuntimeError("could not add %s: %s" % (name, failure))
    sds.rename_subobject(handle, unreal.Text(name))
    return handle


fixture_handle = add(unreal.StaticMeshComponent, "Fixture", root)
fixture = component_of(fixture_handle)
fixture.set_editor_property("static_mesh", eal.load_asset(MESH))
fixture.set_editor_property("relative_scale3d", SCALE)
fixture.set_editor_property("cast_shadow", False)  # the light sits inside it; its own shadow would black out the room

# Replace the earlier Rect Light (a component can't change class)
if "TubeLight" in existing and not isinstance(component_of(existing["TubeLight"]), unreal.PointLightComponent):
    sds.delete_subobject(root, existing.pop("TubeLight"), bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    out("removed the old Rect Light")
light_handle = add(unreal.PointLightComponent, "TubeLight", root)
light = component_of(light_handle)
# Source Length stretches a point light along its X axis: yaw 90 lays it along the tubes (the mesh's Y)
light.set_editor_property("relative_location", unreal.Vector(0, 0, TUBE_Z))
light.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=90))
for key, value in LIGHT.items():
    light.set_editor_property(key, value)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp, False)
out("%s: fixture scale %s, tube light %.0f cm long at Z %.0f, %s cd, class %s" % (BP_PATH, SCALE, LIGHT["source_length"], TUBE_Z, LIGHT["intensity"], light.get_class().get_name()))

# Swap the plain fixtures placed in the range for the Blueprint
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(LEVEL)
bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
swapped = 0
for actor in actors.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    mesh = actor.static_mesh_component.static_mesh
    if not mesh or mesh.get_path_name().split(".")[0] != MESH:
        continue
    scale = actor.get_actor_scale3d()
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    label = actor.get_actor_label()
    new = actors.spawn_actor_from_class(bp_class, location, rotation)
    new.set_actor_scale3d(unreal.Vector(scale.x / SCALE.x, scale.y / SCALE.y, scale.z / SCALE.z))
    new.set_actor_label(label.replace("SM_OverheadLight", "BP_OverheadLight"))
    actors.destroy_actor(actor)
    swapped += 1
    out("swapped %s at %s (scale %s -> %s)" % (label, location, scale, new.get_actor_scale3d()))
levels.save_current_level()
out("%d placed fixtures swapped for BP_OverheadLight" % swapped)
unreal.SystemLibrary.quit_editor()

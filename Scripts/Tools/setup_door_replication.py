"""Makes the template's doors work in co-op: adds our component to each Blueprint and turns on Replicates.

  BP_Door, BP_Outward_Door, BP_SlideDoor, BP_KeypadDoor   UTHDoorSyncComponent
  BP_DoorLock                                             UTHLockedDoorComponent
  BP_Key                                                  UTHKeyPickupComponent
  BP_ValveDoor                                            UTHValveDoorComponent

Their graphs are not touched; the components' headers say how each works. The sounds below are the ones each Blueprint's
own graph plays. Not BP_PhysicDoor: a physics door, which would need its own approach.

Safe to re-run. Build the editor first, so this can see the components. Not headless (compiles and saves Blueprints).
Editor closed:
  UnrealEditor.exe "<abs>\\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>\\Scripts\\Tools\\setup_door_replication.py" -RenderOffScreen -unattended -nosound
"""
import unreal

DOORS = "/Game/Template/Interaction/Blueprint/Door/"
CUES = "/Game/Audio/Cue/"
SETUP = {
    DOORS + "BP_Door": ("DoorSync", unreal.THDoorSyncComponent, {}),
    DOORS + "BP_Outward_Door": ("DoorSync", unreal.THDoorSyncComponent, {}),
    DOORS + "BP_SlideDoor": ("DoorSync", unreal.THDoorSyncComponent, {}),
    DOORS + "BP_KeypadDoor": ("DoorSync", unreal.THDoorSyncComponent, {}),
    DOORS + "BP_DoorLock": ("LockedDoor", unreal.THLockedDoorComponent, {
        "locked_sound": "Lock02_Cue",
        "unlock_sound": "S_Unlock_Door_01_Cue",
        "open_sound": "SimpleDoor_StartOpening_Cue",
        "close_sound": "SimpleDoor_StarClosing_Cue",
    }),
    DOORS + "BP_Key": ("KeyPickup", unreal.THKeyPickupComponent, {"pickup_sound": "SW_Pick_Key_Cue"}),
    "/Game/Template/Interaction/Blueprint/Valve/BP_ValveDoor": ("ValveSync", unreal.THValveDoorComponent, {
        "turn_back_sound": "Open_Valve",
        "turn_open_sound": "Close_Valve",
        "door_move_sound": "MetalDoorOpen",
    }),
}

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

for path, (component_name, component_class, sounds) in SETUP.items():
    bp = eal.load_asset(path)
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    existing = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}
    handle = existing.get(component_name)
    if handle is None:
        handle, failure = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=component_class, blueprint_context=bp))
        if str(failure) not in ("", "None"):
            raise RuntimeError("could not add the component to %s: %s" % (path, failure))
        sds.rename_subobject(handle, unreal.Text(component_name))
    component = lib.get_object(lib.get_data(handle))
    for prop, cue in sounds.items():
        component.set_editor_property(prop, eal.load_asset(CUES + cue))
    # Doors anyone can open don't block the navigation mesh: enemies walk up to them and open them (developer, 2026-11-13).
    # Locked ones (BP_DoorLock, BP_ValveDoor) still do. On the Blueprint's own parts: set on a placed door, its
    # construction script brings them back
    if component_class == unreal.THDoorSyncComponent:
        cleared = 0
        for each in handles:
            part = lib.get_object(lib.get_data(each))
            if isinstance(part, unreal.ActorComponent) and part.get_editor_property("can_ever_affect_navigation"):
                part.set_editor_property("can_ever_affect_navigation", False)
                cleared += 1
        unreal.log_warning("[TH Doors] %s: %d parts no longer block navigation" % (path.split("/")[-1], cleared))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    default = unreal.get_default_object(eal.load_blueprint_class(path))
    default.set_editor_property("replicates", True)
    eal.save_loaded_asset(bp, False)
    unreal.log_warning("[TH Doors] %s: %s, replicates %s, %d sounds" % (path.split("/")[-1], component_name, default.get_editor_property("replicates"), len(sounds)))
unreal.SystemLibrary.quit_editor()

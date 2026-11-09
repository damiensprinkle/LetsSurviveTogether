"""
Target carriers for /Game/Maps/L_FiringRange: every paper target (SM_Target with its hanger SM_Target_Top) gets an
ATHTargetCarrier that moves both along the lane, and the lane's booth gets two ATHTargetButton on its right-hand wall:
SEND (downrange) and RETURN. Hold E on a button to move the target, let go to stop.

  Travel: from NEAR_Y (about 2 m in front of the booth) to FAR_Y (just short of the bullet trap), along -Y.
  Buttons: the template's light switch as a placeholder mesh (SWITCH_MESH); swap it on the placed buttons.
Also makes the targets and hangers Movable (they were Static). Everything it adds is labelled TH_ in the World Outliner
folder TH_RangeTargets: re-running replaces those and nothing else. Editor closed, headless.
"""
import unreal

LEVEL = '/Game/Maps/L_FiringRange'
FOLDER = 'TH_RangeTargets'
SWITCH_MESH = '/Game/Template/Interaction/Models/Switch/SM_SimpleSwitch_01'
NEAR_Y = -820.0     # the booths' fronts are at Y -719
FAR_Y = -3500.0     # the bullet trap starts at Y -3671
BOOTH_WALL_HALF = 8.0  # SM_FireRange_ShootingPlaceWall is 16 cm thick
BUTTON_Y = (-610.0, -640.0)  # SEND, RETURN: on the booth wall beside the counter
BUTTON_Z = 45.0              # chest height (floor at -116)
SWITCH_SCALE = 1.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)


def out(msg):
    unreal.log('setup_target_carriers: ' + msg)


def mesh_name(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    return component.static_mesh.get_name() if component and component.static_mesh else ''


for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith('TH_') and str(actor.get_folder_path()) == FOLDER:
        actors.destroy_actor(actor)

all_actors = actors.get_all_level_actors()
targets = [a for a in all_actors if mesh_name(a) == 'SM_Target']
tops = [a for a in all_actors if mesh_name(a) == 'SM_Target_Top']
walls = [a for a in all_actors if mesh_name(a) == 'SM_FireRange_ShootingPlaceWall']
switch = unreal.load_asset(SWITCH_MESH)
out('switch mesh bounds %s' % switch.get_bounds())

for lane, target in enumerate(sorted(targets, key=lambda a: a.get_actor_location().x)):
    location = target.get_actor_location()
    top = min(tops, key=lambda a: (a.get_actor_location() - location).length())
    for part in (target, top):
        part.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    carrier = actors.spawn_actor_from_class(unreal.THTargetCarrier, location, unreal.Rotator(0, 0, 0))
    carrier.set_actor_label('TH_Carrier%d' % (lane + 1))
    carrier.set_folder_path(FOLDER)
    carrier.set_editor_property('parts', [target, top])
    # Offsets are along -Y from where the target hangs now
    carrier.set_editor_property('min_offset', location.y - NEAR_Y)
    carrier.set_editor_property('max_offset', location.y - FAR_Y)

    # The booth's right-hand wall (facing downrange, right is +X): the nearest wall past the lane's middle
    wall = min([w for w in walls if w.get_actor_location().x > location.x], key=lambda w: w.get_actor_location().x)
    face_x = wall.get_actor_location().x - BOOTH_WALL_HALF - 1.0
    for kind, (label, direction, y) in enumerate((('SEND', 1, BUTTON_Y[0]), ('RETURN', -1, BUTTON_Y[1]))):
        button = actors.spawn_actor_from_class(unreal.THTargetButton, unreal.Vector(face_x, y, BUTTON_Z), unreal.Rotator(0, 0, 180))
        button.set_actor_label('TH_Carrier%d_%s' % (lane + 1, label.title()))
        button.set_folder_path(FOLDER)
        press = button.get_component_by_class(unreal.THTargetButtonComponent)
        press.set_editor_property('carrier', carrier)
        press.set_editor_property('direction', direction)
        mesh = [c for c in button.get_components_by_class(unreal.StaticMeshComponent)][0]
        mesh.set_static_mesh(switch)
        mesh.set_relative_scale3d(unreal.Vector(SWITCH_SCALE, SWITCH_SCALE, SWITCH_SCALE))
        # The switch is a 20 cm plate facing its +Y, with its back at Y 0: turn it to face the button's +X, out of the wall
        mesh.set_relative_rotation(unreal.Rotator(0, 0, -90), False, False)
        text = button.get_component_by_class(unreal.TextRenderComponent)
        text.set_editor_property('text', label)
        text.set_relative_location(unreal.Vector(1.0, 0.0, 14.0), False, False)
    out('lane %d: %s and %s, travel Y %.0f..%.0f, buttons on %s at X %.0f' % (lane + 1, target.get_actor_label(), top.get_actor_label(),
        NEAR_Y, FAR_Y, wall.get_actor_label(), face_x))

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError('Could not save ' + LEVEL)
out('saved')

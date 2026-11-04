"""Sets up the flashlights.

  Hand flashlight (ATHHandFlashlight, key 3 once picked up):
    /Game/Weapons/Data/DA_HandFlashlight          weapon data: the FP Flashlight Animations pack's arms animations
    /Game/Characters/BodyAnimSets/DA_Body_Flashlight  what other players see: Lyra's flashlight poses
  Weapon light (L, under the barrel):
    DA_M4 / DA_G17 Weapon Light section: the Fab "FREE Tactical Flashlight" mesh, placed from each gun's muzzle bone

Safe to re-run; it overwrites the values below. Tune the light placement live with th.WeaponLightTune, then copy the
numbers into WEAPON_LIGHTS here (or into the data asset, and stop re-running this for that gun).
"""
import unreal

eal = unreal.EditorAssetLibrary
DATA = "/Game/Weapons/Data"
BODY = "/Game/Characters/BodyAnimSets"
FP = "/Game/FPFlashlightAnims/Animations/"
LYF = "/Game/Blueprint/Lyra_Content/Characters/Mannequin/Animations/Locomotion/FlashLight/"
CLICK = "/Game/Audio/Cue/FlashlightButton_Cue"
LIGHT_MESH = "/Game/Fab/FREE_Tactical_Flashlight_-_Game_Ready/SM_Attach_Flashlight_01"


def out(msg):
    unreal.log_warning("[TH Flashlight] " + msg)


def load(path):
    asset = eal.load_asset(path) if path else None
    if path and not asset:
        out("missing " + path)
    return asset


def get_or_create(folder, name, cls):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls.static_class())
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def frame_along(forward, up):
    """Rotation whose X axis is `forward` and Z axis is `up` (both in the parent bone's space)."""
    return unreal.MathLibrary.make_rot_from_xz(forward, up)


# ---- Third-person body set: Lyra's flashlight poses (same skeleton as the body) ----
body = get_or_create(BODY, "DA_Body_Flashlight", unreal.THBodyAnimSet)
unarmed = load(BODY + "/DA_Body_Unarmed")
for prop in ["walk_backward", "walk_left", "walk_right", "crouch_idle", "crouch_forward", "crouch_backward",
             "crouch_left", "crouch_right", "falling"]:
    body.set_editor_property(prop, None)
body.set_editor_property("idle", load(LYF + "A_Flashlight_CC"))
body.set_editor_property("walk_forward", load(LYF + "A_RH_Flashlight_T2_Walk_Fwd"))
# Other directions and crouching borrow the unarmed legs; this keeps the flashlight held on the upper body
body.set_editor_property("upper_body_pose", load(LYF + "A_Flashlight_CC"))
body.set_editor_property("hand_ik_weight", 0.0)
# Draw: the pistol draw (one hand from the hip); the flashlight shows once the hand is up, as with the guns
pistol = load(BODY + "/DA_Body_Pistol")
if pistol:
    body.set_editor_property("equip", pistol.get_editor_property("equip"))
    body.set_editor_property("equip_reveal_fraction", pistol.get_editor_property("equip_reveal_fraction"))
    body.set_editor_property("action_blend_time", pistol.get_editor_property("action_blend_time"))
body.set_editor_property("upper_body_yaw", 0.0)
body.set_editor_property("upper_body_yaw_head_bone", "None")
if unarmed:
    body.set_editor_property("aim_offset", unarmed.get_editor_property("aim_offset"))
    body.set_editor_property("aim_offset_pitch_at_look_up", unarmed.get_editor_property("aim_offset_pitch_at_look_up"))
eal.save_loaded_asset(body)
out("DA_Body_Flashlight saved")

# ---- Hand flashlight weapon data ----
data = get_or_create(DATA, "DA_HandFlashlight", unreal.THWeaponData)
data.set_editor_property("idle_animation", load(FP + "AS_Flashlight_Idle"))
data.set_editor_property("equip_animation", load(FP + "AS_Flashlight_Eqiup"))
data.set_editor_property("fire_animation", None)
data.set_editor_property("reload_animation", None)
data.set_editor_property("reload_empty_animation", None)
data.set_editor_property("equip_duration", 0.6)
data.set_editor_property("magazine_size", 1)
data.set_editor_property("starting_reserve_ammo", 0)
data.set_editor_property("fire_modes", [unreal.THFireMode.SEMI_AUTO])
data.set_editor_property("move_speed_multiplier", 1.0)
data.set_editor_property("aim_move_speed_multiplier", 1.0)
# The pack's demo character holds its arms here relative to the camera
data.set_editor_property("camera_hold_offset", unreal.Transform(vec(-10.0, 0.0, -162.7), unreal.Rotator(0.0, 0.0, -90.0), vec(1, 1, 1)))
data.set_editor_property("body_anim_set", body)
data.set_editor_property("muzzle_light_intensity", 0.0)
data.set_editor_property("muzzle_flash_material", None)
data.set_editor_property("shell_mesh", None)
data.set_editor_property("fire_sounds", [])
data.set_editor_property("weapon_light_click_sound", load(CLICK))
eal.save_loaded_asset(data)
out("DA_HandFlashlight saved")

# ---- Weapon light on the guns ----
# Frames are in the muzzle bone's space. On both rigs that bone's +Z runs down the barrel and -Y is up.
# The mesh is long along its Y and its mounting face is +X; WeaponLightMeshTransform turns that so the lens faces
# forward (frame X) and the light hangs below the rail.
mesh = load(LIGHT_MESH)
yaw = unreal.Quat()
yaw.set_from_euler(vec(0.0, 0.0, -90.0))       # mesh +Y -> forward
roll = unreal.Quat()
roll.set_from_euler(vec(-90.0, 0.0, 0.0))      # then hang it below
mesh_rot = (roll * yaw).rotator()

DEFAULT_FRAME = frame_along(vec(0.0, 0.0, 1.0), vec(0.0, -1.0, 0.0))
WEAPON_LIGHTS = {
    # gun: (attach location in muzzle-bone space, attach rotation (None = the default frame above), mesh scale,
    #       lens offset in the light's frame)
    "DA_M4": (vec(0.0, 4.2, 27.0), None, 1.0, vec(10.5, -0.8, -2.0)),
    # Placed by the developer in the editor (2026-11-04): under the slide; pitch 90, yaw -90, roll 0
    "DA_G17": (vec(1.5, 0.0, 9.0), unreal.Rotator(roll=0.0, pitch=90.0, yaw=-90.0), 0.8, vec(8.4, -0.6, -1.6)),
}
for name, (location, rotation, scale, lens) in WEAPON_LIGHTS.items():
    gun = load(f"{DATA}/{name}")
    if not gun:
        continue
    attach_rot = rotation if rotation else DEFAULT_FRAME
    gun.set_editor_property("weapon_light_mesh", mesh)
    gun.set_editor_property("weapon_light_attach", unreal.Transform(location, attach_rot, vec(1, 1, 1)))
    gun.set_editor_property("weapon_light_mesh_transform", unreal.Transform(vec(0, 0, 0), mesh_rot, vec(scale, scale, scale)))
    gun.set_editor_property("weapon_light_lens_offset", lens)
    gun.set_editor_property("weapon_light_click_sound", load(CLICK))
    eal.save_loaded_asset(gun)
    out(f"{name} weapon light set: attach {location} rot {attach_rot} mesh rot {mesh_rot}")

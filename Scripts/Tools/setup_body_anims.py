"""Creates the third-person body animation sets and hooks them up.

  /Game/Characters/BodyAnimSets/DA_Body_Unarmed, DA_Body_Rifle, DA_Body_Pistol (UTHBodyAnimSet)
  DA_M4.BodyAnimSet = Rifle, DA_G17.BodyAnimSet = Pistol, and their Third Person Anchor Offset (table near the end)
  BP_FirstPersonCharacter's THWeapons.UnarmedBodyAnimSet = Unarmed

Legs come from Lyra's UE5 male unarmed walk/crouch for every set (the project's rifle/pistol walks are either
female or on the UE4 skeleton). Weapon sets layer an aiming pose on the upper body.
Safe to re-run; it overwrites the sets' animation slots with the table below.
"""
import unreal

eal = unreal.EditorAssetLibrary
FOLDER = "/Game/Characters/BodyAnimSets"
LY = "/Game/Blueprint/Lyra_Content/Characters/Mannequin/Animations/"
LYU = LY + "Locomotion/Unarmed/"
CM = "/Game/Characters/Mannequins/Anims/"
SAP = "/Game/SampleAnimationPack/Animations/Rifle/"


def out(msg):
    unreal.log_warning("[TH Body] " + msg)


LEGS = {
    "idle": LYU + "MM_Unarmed_Idle_Ready",
    "walk_forward": LYU + "MM_Unarmed_Walk_Fwd",
    "walk_backward": LYU + "MM_Unarmed_Walk_Bwd",
    "walk_left": LYU + "MM_Unarmed_Walk_Left",
    "walk_right": LYU + "MM_Unarmed_Walk_Right",
    "crouch_idle": LYU + "MM_Unarmed_Crouch_Idle",
    "crouch_forward": LYU + "MM_Unarmed_Crouch_Walk_Fwd",
    "crouch_backward": LYU + "MM_Unarmed_Crouch_Walk_Bwd",
    "crouch_left": LYU + "MM_Unarmed_Crouch_Walk_Left",
    "crouch_right": LYU + "MM_Unarmed_Crouch_Walk_Right",
    "falling": LYU + "MM_Unarmed_Jump_Fall_Loop",
}

SETS = {
    "DA_Body_Unarmed": dict(LEGS, **{
        "aim_offset": LY + "AimOffsets/AO_MM_Unarmed_Idle_Ready",
        "aim_offset_pitch_at_look_up": 90.0,
    }),
    # Rifle: the SampleAnimationPack's aiming idle and walks (full body, so the upper body moves with the legs).
    # It has no crouch, so crouching uses the unarmed crouch with the aiming pose layered on the upper body.
    "DA_Body_Rifle": {
        "idle": SAP + "AS_F_ADS_Rifle_Idle_01",
        "walk_forward": SAP + "AS_F_ADS_Rifle_Walk_F_Loop",
        "walk_backward": SAP + "AS_F_ADS_Rifle_Walk_B_Loop",
        "walk_left": SAP + "AS_F_ADS_Rifle_Walk_L_Loop",
        "walk_right": SAP + "AS_F_ADS_Rifle_Walk_R_Loop",
        "upper_body_pose": SAP + "AS_F_ADS_Rifle_Idle_01",
        "aim_offset": CM + "Rifle/AIM/AO_Rifle",
        "aim_offset_pitch_at_look_up": 1.0,
        # Fire is additive (a kick); reload and equip are full poses that replace the upper body while they play.
        # Lyra's *_Additive versions assume Lyra's hip-fire pose and throw the arms around on top of this aiming pose.
        "fire": LY + "Actions/MM_Rifle_Fire",
        "reload": LY + "Actions/MM_Rifle_Reload",
        # Draw: the gun stays hidden for the first part (equip_reveal_fraction), so it doesn't swing round behind the
        # shoulder, and appears once the hand comes forward
        "equip": LY + "Actions/MM_Rifle_Equip",
        "equip_reveal_fraction": 0.6,
        # The aiming animations stand bladed (torso ~28 degrees to the right); square it up so walking forward looks forward
        "upper_body_yaw": -28.0,
        # The head is bladed with the torso in these animations, so it turns with it (no counter-turn on the neck)
        "upper_body_yaw_head_bone": "None",
        # Fingers for both hands: the developer's copy of the aiming idle, checked against a preview rifle
        "hand_pose": SAP + "AS_F_ADS_Rifle_Idle_02",
        "hand_pose_time": 0.0,
        "hand_pose_right": True,
        "hand_pose_left": True,
    },
    # Rifle carried low (not aiming, no shot for Ready Hold Time): the pack's plain rifle idle and walks, as they are.
    # Linked from DA_Body_Rifle (Lowered Set) below. The gun rides its right hand; the left hand is the animation's.
    "DA_Body_Rifle_Lowered": {
        "idle": SAP + "AS_F_Rifle_Idle_01",
        "walk_forward": SAP + "AS_F_Rifle_Walk_F_Loop",
        "walk_backward": SAP + "AS_F_Rifle_Walk_B_Loop",
        "walk_left": SAP + "AS_F_Rifle_Walk_L_Loop",
        "walk_right": SAP + "AS_F_Rifle_Walk_R_Loop",
        "upper_body_pose": SAP + "AS_F_Rifle_Idle_01",
    },
    # Pistol carried lower: Lyra's hip-fire idle, retargeted from the UE4 mannequin (retarget_pistol_idle.py). No
    # walks of its own, so it walks on the unarmed legs with this pose layered on the upper body.
    "DA_Body_Pistol_Lowered": {
        "idle": CM + "Pistol/MM_Pistol_Idle_Hipfire",
        "upper_body_pose": CM + "Pistol/MM_Pistol_Idle_Hipfire",
        # The developer poses the left hand in the idle itself, so it isn't pulled onto the pistol
        "left_hand_on_weapon": False,
    },
    # Pistol raised (aiming, or shot in the last 7 s): the template's pistol aiming idle, hands as the animation has
    # them (hand_ik_weight 0: the developer poses them in MF_Pistol_Idle_ADS). No walks of its own, so while moving
    # it walks on the unarmed legs with that pose layered on top and the posed hands stay.
    "DA_Body_Pistol": {
        "idle": CM + "Pistol/MF_Pistol_Idle_ADS",
        "walk_forward": None,
        "walk_backward": None,
        "walk_left": None,
        "walk_right": None,
        "hand_ik_weight": 0.0,
        "upper_body_pose": CM + "Pistol/MF_Pistol_Idle_ADS",
        "aim_offset": CM + "Pistol/Aim/AO_Pistol",
        "aim_offset_pitch_at_look_up": 1.0,
        "fire": LY + "Actions/MM_Pistol_Fire",
        "reload": LY + "Actions/MM_Pistol_Reload",
        "equip": LY + "Actions/MM_Pistol_Equip",
        "equip_reveal_fraction": 0.6,
        # Fingers as in the pistol aiming animation itself (copying the first-person fingers malformed them)
        "hand_pose": CM + "Pistol/MF_Pistol_Idle_ADS",
        "hand_pose_time": 0.0,
        "hand_pose_right": True,
        "hand_pose_left": True,
    },
}


def get_or_create(name):
    path = f"{FOLDER}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.THBodyAnimSet.static_class())
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.THBodyAnimSet, factory)


# Lyra's crouch walks move at 300 cm/s and ours at 115: they need to slow to ~0.4x to keep the feet planted
for values in SETS.values():
    values["play_rate_range"] = unreal.Vector2D(0.35, 1.6)

assets = {}
for name, values in SETS.items():
    asset = get_or_create(name)
    for prop, value in values.items():
        if prop.endswith("_bone"):
            value = unreal.Name(value)
        elif isinstance(value, str):
            loaded = eal.load_asset(value)
            if not loaded:
                out(f"MISSING {value} for {name}.{prop}")
                continue
            value = loaded
        if value is None and prop == "hand_pose":
            asset.set_editor_property("hand_pose_right", False)
            asset.set_editor_property("hand_pose_left", False)
        asset.set_editor_property(prop, value)
    out(f"{name} saved={eal.save_loaded_asset(asset, False)}")
    assets[name] = asset

# Rifle: lowered carry when not aiming and no shot for 7 s
rifle = assets["DA_Body_Rifle"]
rifle.set_editor_property("lowered_set", assets["DA_Body_Rifle_Lowered"])
rifle.set_editor_property("ready_hold_time", 7.0)
out(f"DA_Body_Rifle lowered set, saved={eal.save_loaded_asset(rifle, False)}")
pistol = assets["DA_Body_Pistol"]
pistol.set_editor_property("lowered_set", assets["DA_Body_Pistol_Lowered"])
pistol.set_editor_property("ready_hold_time", 7.0)
out(f"DA_Body_Pistol lowered set, saved={eal.save_loaded_asset(pistol, False)}")

# Third-person hold, in aim space (X forward, Y right, Z up; cm and degrees):
#   anchor: where the weapon's grip (right hand) sits from the head. Rifle: stock in the shoulder, sights at the eye.
#   The hands copy the weapon's first-person hands (palm, angle, fingers);
#   right/left nudge each hand from there; right_rot/left_rot turn it (pitch, yaw, roll).
#   hidden: rig bones hidden on the third-person copy (the G17's spare magazine, Mag1, sits at the chest when held).
# Tune live in play with th.ThirdPersonTune, then copy the sums here.
WEAPONS = {
    "/Game/Weapons/Data/DA_M4": dict(set="DA_Body_Rifle", anchor=(13, 19, -19), rotation=(0, 0, 0), left_shoulder=(8, 3, 0),
                                     right=(0, 0, 0), right_rot=(0, 0, 0), left=(-3, 2, 0), left_rot=(0, 5, 0)),
    "/Game/Weapons/Data/DA_G17": dict(set="DA_Body_Pistol", anchor=(40, 2, -14), hidden=["Mag1"],
                                      right=(0, 0, 0), right_rot=(0, 0, 0), left=(0, 0, 0), left_rot=(0, 0, 0)),
}
for data_path, w in WEAPONS.items():
    data = eal.load_asset(data_path)
    data.set_editor_property("body_anim_set", assets[w["set"]])
    data.set_editor_property("third_person_anchor_offset", unreal.Vector(*w["anchor"]))
    data.set_editor_property("third_person_left_hand_follows_right", w.get("left_follows_right", False))
    data.set_editor_property("third_person_match_rig_hands", True)
    data.set_editor_property("third_person_right_shoulder_offset", unreal.Vector(*w.get("right_shoulder", (0, 0, 0))))
    data.set_editor_property("third_person_left_shoulder_offset", unreal.Vector(*w.get("left_shoulder", (0, 0, 0))))
    rot = w.get("rotation", (0, 0, 0))  # (pitch, yaw, roll); the editor shows these as Y, Z, X
    data.set_editor_property("third_person_rotation_offset", unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    if "hidden" in w:
        data.set_editor_property("third_person_hidden_bones", w["hidden"])
    data.set_editor_property("third_person_right_hand_offset", unreal.Vector(*w["right"]))
    data.set_editor_property("third_person_left_hand_offset", unreal.Vector(*w["left"]))
    data.set_editor_property("third_person_right_hand_rotation", unreal.Rotator(roll=w["right_rot"][2], pitch=w["right_rot"][0], yaw=w["right_rot"][1]))
    data.set_editor_property("third_person_left_hand_rotation", unreal.Rotator(roll=w["left_rot"][2], pitch=w["left_rot"][0], yaw=w["left_rot"][1]))
    out(f"{data_path}: {w} saved={eal.save_loaded_asset(data, False)}")

char = eal.load_asset("/Game/Blueprint/Character/BP_FirstPersonCharacter")
wc = unreal.get_default_object(char.generated_class()).get_editor_property("weapon_component")
wc.set_editor_property("unarmed_body_anim_set", assets["DA_Body_Unarmed"])
out(f"BP_FirstPersonCharacter unarmed set, saved={eal.save_loaded_asset(char, False)}")

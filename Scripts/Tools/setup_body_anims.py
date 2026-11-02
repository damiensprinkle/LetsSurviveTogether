"""Creates the third-person body animation sets and hooks them up.

  /Game/Characters/BodyAnimSets/DA_Body_Unarmed, DA_Body_Rifle, DA_Body_Pistol (UTHBodyAnimSet)
  DA_M4.BodyAnimSet = Rifle, DA_G17.BodyAnimSet = Pistol
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
    # Weapon sets have no legs of their own: the unarmed set's are used
    "DA_Body_Rifle": {
        "upper_body_pose": CM + "Rifle/MF_Rifle_Idle_ADS",
        "aim_offset": CM + "Rifle/AIM/AO_Rifle",
        "aim_offset_pitch_at_look_up": 1.0,
        # Fire is additive (a kick); reload and equip are full poses that replace the upper body while they play.
        # Lyra's *_Additive versions assume Lyra's hip-fire pose and throw the arms around on top of this aiming pose.
        "fire": LY + "Actions/MM_Rifle_Fire",
        "reload": LY + "Actions/MM_Rifle_Reload",
        "equip": LY + "Actions/MM_Rifle_Equip",
    },
    "DA_Body_Pistol": {
        "upper_body_pose": CM + "Pistol/MF_Pistol_Idle_ADS",
        "aim_offset": CM + "Pistol/Aim/AO_Pistol",
        "aim_offset_pitch_at_look_up": 1.0,
        "fire": LY + "Actions/MM_Pistol_Fire",
        "reload": LY + "Actions/MM_Pistol_Reload",
        "equip": LY + "Actions/MM_Pistol_Equip",
    },
}


def get_or_create(name):
    path = f"{FOLDER}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.THBodyAnimSet.static_class())
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.THBodyAnimSet, factory)


assets = {}
for name, values in SETS.items():
    asset = get_or_create(name)
    for prop, value in values.items():
        if isinstance(value, str):
            loaded = eal.load_asset(value)
            if not loaded:
                out(f"MISSING {value} for {name}.{prop}")
                continue
            value = loaded
        asset.set_editor_property(prop, value)
    out(f"{name} saved={eal.save_loaded_asset(asset, False)}")
    assets[name] = asset

for data_path, set_name in [("/Game/Weapons/Data/DA_M4", "DA_Body_Rifle"), ("/Game/Weapons/Data/DA_G17", "DA_Body_Pistol")]:
    data = eal.load_asset(data_path)
    data.set_editor_property("body_anim_set", assets[set_name])
    out(f"{data_path}.BodyAnimSet = {set_name} saved={eal.save_loaded_asset(data, False)}")

char = eal.load_asset("/Game/Blueprint/Character/BP_FirstPersonCharacter")
wc = unreal.get_default_object(char.generated_class()).get_editor_property("weapon_component")
wc.set_editor_property("unarmed_body_anim_set", assets["DA_Body_Unarmed"])
out(f"BP_FirstPersonCharacter unarmed set, saved={eal.save_loaded_asset(char, False)}")

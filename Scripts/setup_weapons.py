"""
One-time editor setup for the C++ foundation. Safe to re-run: existing assets are updated, not duplicated.

Run headless:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<path>/setup_weapons.py" -nullrhi -unattended

- Reparents BP_FirstPersonCharacter -> THCharacter and BP_FirstPersonGameMode -> THGameMode
- Creates DA_Rifle / DA_Pistol (THWeaponData) and BP_Weapon_Rifle / BP_Weapon_Pistol (THWeaponBase)
- Gives the character a rifle + pistol default loadout
"""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
bel = unreal.BlueprintEditorLibrary

DATA_DIR = "/Game/Weapons/Data"
BP_DIR = "/Game/Weapons/Blueprints"
FIRE_SOUND = "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02"


def log(msg):
    unreal.log("[TH Setup] " + msg)


def load_or_create_data(name):
    path = f"{DATA_DIR}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.THWeaponData)
    return asset_tools.create_asset(name, DATA_DIR, unreal.THWeaponData, factory)


def load_or_create_bp(name, parent):
    path = f"{BP_DIR}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return asset_tools.create_asset(name, BP_DIR, None, factory)


def v2(x, y):
    return unreal.Vector2D(x, y)


def configure(data, values):
    for key, value in values.items():
        data.set_editor_property(key, value)
    eal.save_loaded_asset(data)


fire_sound = eal.load_asset(FIRE_SOUND) if eal.does_asset_exist(FIRE_SOUND) else None

# Rifle: M4-style. Pattern climbs for ~10 shots, then sways left/right. Learnable, not random.
rifle_pattern = [
    v2(0.00, 0.55), v2(0.05, 0.65), v2(-0.05, 0.75), v2(0.10, 0.80), v2(0.00, 0.80),
    v2(-0.15, 0.70), v2(-0.20, 0.60), v2(-0.30, 0.45), v2(-0.35, 0.30), v2(-0.30, 0.20),
    v2(0.25, 0.15), v2(0.40, 0.10), v2(0.45, 0.05), v2(0.40, 0.05), v2(0.30, 0.05),
    v2(0.10, 0.05), v2(-0.30, 0.05), v2(-0.40, 0.00), v2(-0.40, 0.00), v2(-0.30, 0.05),
    v2(0.20, 0.05), v2(0.35, 0.00), v2(0.35, 0.00), v2(0.20, 0.05), v2(-0.20, 0.05),
    v2(-0.35, 0.00), v2(-0.30, 0.00), v2(0.10, 0.05), v2(0.30, 0.00), v2(0.30, 0.00),
]

rifle_data = load_or_create_data("DA_Rifle")
configure(rifle_data, {
    "display_name": "Assault Rifle",
    "slot": unreal.THWeaponSlot.PRIMARY,
    "damage": 33.0,
    "headshot_multiplier": 4.0,
    "rounds_per_minute": 666.0,
    "fire_modes": [unreal.THFireMode.FULL_AUTO, unreal.THFireMode.SEMI_AUTO],
    "magazine_size": 30,
    "starting_reserve_ammo": 90,
    "reload_duration": 3.1,
    "equip_duration": 0.6,
    "standing_inaccuracy": 0.15,
    "moving_inaccuracy": 5.0,
    "recoil_pattern": rifle_pattern,
    "recoil_reset_delay": 0.4,
    "view_recovery_rate": 12.0,
    "aim_field_of_view": 55.0,
    "move_speed_multiplier": 0.9,
    "camera_hold_offset": unreal.Transform(location=[35.0, 14.0, -16.0], rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0)),
    "fire_sound": fire_sound,
})

pistol_data = load_or_create_data("DA_Pistol")
configure(pistol_data, {
    "display_name": "Pistol",
    "slot": unreal.THWeaponSlot.SECONDARY,
    "damage": 26.0,
    "headshot_multiplier": 4.0,
    "rounds_per_minute": 400.0,
    "fire_modes": [unreal.THFireMode.SEMI_AUTO],
    "magazine_size": 20,
    "starting_reserve_ammo": 60,
    "reload_duration": 2.2,
    "equip_duration": 0.4,
    "standing_inaccuracy": 0.3,
    "moving_inaccuracy": 2.5,
    "recoil_pattern": [v2(0.0, 1.2), v2(0.1, 1.3), v2(-0.1, 1.4), v2(0.15, 1.5), v2(-0.15, 1.5)],
    "recoil_reset_delay": 0.3,
    "view_recovery_rate": 20.0,
    "aim_field_of_view": 70.0,
    "move_speed_multiplier": 1.0,
    "camera_hold_offset": unreal.Transform(location=[30.0, 12.0, -12.0], rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)),
    "fire_sound": fire_sound,
})


def make_weapon_bp(name, data, mesh_path):
    bp = load_or_create_bp(name, unreal.THWeaponBase)
    bel.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("weapon_data", data)
    if eal.does_asset_exist(mesh_path):
        cdo.get_editor_property("mesh").set_editor_property("skeletal_mesh_asset", eal.load_asset(mesh_path))
    else:
        unreal.log_warning(f"[TH Setup] Mesh missing: {mesh_path}")
    eal.save_loaded_asset(bp)
    log(f"Configured {name}")
    return bp


rifle_bp = make_weapon_bp("BP_Weapon_Rifle", rifle_data, "/Game/Weapons/Rifle/Meshes/SKM_Rifle")
pistol_bp = make_weapon_bp("BP_Weapon_Pistol", pistol_data, "/Game/Weapons/Pistol/Meshes/SKM_Pistol")


def reparent(path, new_parent):
    bp = eal.load_asset(path)
    if bp is None:
        unreal.log_error(f"[TH Setup] Not found: {path}")
        return None
    current = bp.generated_class().get_super_class() if hasattr(bp.generated_class(), "get_super_class") else None
    ok = bel.reparent_blueprint(bp, new_parent)
    bel.compile_blueprint(bp)
    log(f"Reparented {path} -> {new_parent.get_name()} (was {current.get_name() if current else '?'}), ok={ok}")
    return bp


char_bp = reparent("/Game/Blueprint/Character/BP_FirstPersonCharacter", unreal.THCharacter.static_class())
if char_bp:
    char_cdo = unreal.get_default_object(char_bp.generated_class())
    weapons = char_cdo.get_editor_property("weapon_component")
    weapons.set_editor_property("default_loadout", [rifle_bp.generated_class(), pistol_bp.generated_class()])
    eal.save_loaded_asset(char_bp)
    log("Character loadout: rifle, pistol")

mode_bp = reparent("/Game/Blueprint/Character/BP_FirstPersonGameMode", unreal.THGameMode.static_class())
if mode_bp:
    eal.save_loaded_asset(mode_bp)

log("Done")

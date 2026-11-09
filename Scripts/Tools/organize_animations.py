"""Animation cleanup (2026-11-05): every animation the game plays gets one home and one name pattern.

  First person:  /Game/Weapons/<Gun>/Animations/A_FP_<Gun>_<Action>
  Third person:  /Game/Animations/ThirdPerson/<Set>/A_TP_<Set>_<State>_<Action>, aim offsets AO_TP_<Set>
  Body sets:     /Game/Characters/BodyAnimSets/DA_Body_<Set>[_Lowered], one per gun

Animations from packs (Lyra, Sample Animation Pack, the UE template's mannequin folder, FP flashlight pack) are COPIED:
the packs stay whole (their own Blueprints use the originals). Our own (the M4 / G17 first-person imports, the
retargeted pistol hip-fire idle, the old per-type body sets) are copied, repointed, then deleted (a move without
redirectors). The data assets (DA_M4, DA_G17, DA_HandFlashlight, the body sets) are pointed at the copies; aim offsets
are copied with their samples and repointed to the copied samples.

Run headless with the editor closed, in two steps (the second needs a fresh asset registry):
  TH_STEP=copy    copy, create the new body sets, repoint (data assets, aim offset samples, additive base poses), save
  TH_STEP=bases   only the additive base poses (part of copy)
  TH_STEP=delete  delete our old originals once nothing refers to them
  TH_DRY=1        print only
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
STEP = os.environ.get("TH_STEP", "")
DRY = os.environ.get("TH_DRY") == "1"

LYRA = "/Game/ThirdParty/Lyra/Characters/Mannequin/Animations/"
SAP = "/Game/ThirdParty/SampleAnimationPack/Animations/Rifle/"
TPL = "/Game/Characters/Mannequins/Anims/"
FPF = "/Game/ThirdParty/FPFlashlightAnims/Animations/"
TP = "/Game/Animations/ThirdPerson/"

# Source -> new path. MOVE = ours: the source is deleted afterwards.
COPY = {}
MOVE = {}

def tp(set_name, items, base):
    for src, name in items:
        COPY[base + src] = "%s%s/%s" % (TP, set_name, name)

tp("Unarmed", [
    ("Locomotion/Unarmed/MM_Unarmed_Idle_Ready", "A_TP_Unarmed_Idle"),
    ("Locomotion/Unarmed/MM_Unarmed_Walk_Fwd", "A_TP_Unarmed_Walk_F"),
    ("Locomotion/Unarmed/MM_Unarmed_Walk_Bwd", "A_TP_Unarmed_Walk_B"),
    ("Locomotion/Unarmed/MM_Unarmed_Walk_Left", "A_TP_Unarmed_Walk_L"),
    ("Locomotion/Unarmed/MM_Unarmed_Walk_Right", "A_TP_Unarmed_Walk_R"),
    ("Locomotion/Unarmed/MM_Unarmed_Crouch_Idle", "A_TP_Unarmed_Crouch_Idle"),
    ("Locomotion/Unarmed/MM_Unarmed_Crouch_Walk_Fwd", "A_TP_Unarmed_Crouch_Walk_F"),
    ("Locomotion/Unarmed/MM_Unarmed_Crouch_Walk_Bwd", "A_TP_Unarmed_Crouch_Walk_B"),
    ("Locomotion/Unarmed/MM_Unarmed_Crouch_Walk_Left", "A_TP_Unarmed_Crouch_Walk_L"),
    ("Locomotion/Unarmed/MM_Unarmed_Crouch_Walk_Right", "A_TP_Unarmed_Crouch_Walk_R"),
    ("Locomotion/Unarmed/MM_Unarmed_Jump_Fall_Loop", "A_TP_Unarmed_Fall"),
    ("AimOffsets/AO_MM_Unarmed_Idle_Ready", "AO_TP_Unarmed"),
] + [("AimOffsets/MM_Unarmed_Idle_Ready_AO_" + d, "A_TP_Unarmed_AO_" + d)
     for d in ("CC", "CD", "CU", "LBC", "LBD", "LBU", "LC", "LD", "LU", "RBC", "RBD", "RBU", "RC", "RD", "RU")], LYRA)

tp("M4", [
    ("AS_F_ADS_Rifle_Idle_01", "A_TP_M4_Raised_Idle"),
    ("AS_F_ADS_Rifle_Walk_F_Loop", "A_TP_M4_Raised_Walk_F"),
    ("AS_F_ADS_Rifle_Walk_B_Loop", "A_TP_M4_Raised_Walk_B"),
    ("AS_F_ADS_Rifle_Walk_L_Loop", "A_TP_M4_Raised_Walk_L"),
    ("AS_F_ADS_Rifle_Walk_R_Loop", "A_TP_M4_Raised_Walk_R"),
    ("AS_F_ADS_Rifle_Idle_02", "A_TP_M4_HandPose"),
    ("AS_F_Rifle_Idle_01", "A_TP_M4_Lowered_Idle"),
    ("AS_F_Rifle_Walk_F_Loop", "A_TP_M4_Lowered_Walk_F"),
    ("AS_F_Rifle_Walk_B_Loop", "A_TP_M4_Lowered_Walk_B"),
    ("AS_F_Rifle_Walk_L_Loop", "A_TP_M4_Lowered_Walk_L"),
    ("AS_F_Rifle_Walk_R_Loop", "A_TP_M4_Lowered_Walk_R"),
], SAP)
tp("M4", [
    ("Actions/MM_Rifle_Fire", "A_TP_M4_Fire"),
    ("Actions/MM_Rifle_Reload", "A_TP_M4_Reload"),
    ("Actions/MM_Rifle_Equip", "A_TP_M4_Equip"),
], LYRA)
tp("M4", [
    ("Rifle/AIM/AO_Rifle", "AO_TP_M4"),
    ("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CC", "A_TP_M4_AO_CC"),
    ("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CD", "A_TP_M4_AO_CD"),
    ("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CU", "A_TP_M4_AO_CU"),
    ("Rifle/MF_Rifle_Idle_ADS", "A_TP_M4_AO_Base"),
], TPL)

tp("G17", [
    ("Pistol/MF_Pistol_Idle_ADS", "A_TP_G17_Raised_Idle"),
    ("Pistol/Aim/AO_Pistol", "AO_TP_G17"),
    ("Pistol/Aim/MF_Pistol_Idle_ADS1", "A_TP_G17_AO_CC"),
    ("Pistol/Aim/MF_Pistol_Idle_ADS_AO_CD", "A_TP_G17_AO_CD"),
    ("Pistol/Aim/MF_Pistol_Idle_ADS_AO_CU", "A_TP_G17_AO_CU"),
], TPL)
tp("G17", [
    ("Actions/MM_Pistol_Fire", "A_TP_G17_Fire"),
    ("Actions/MM_Pistol_Reload", "A_TP_G17_Reload"),
    ("Actions/MM_Pistol_Equip", "A_TP_G17_Equip"),
], LYRA)
MOVE[TPL + "Pistol/MM_Pistol_Idle_Hipfire"] = TP + "G17/A_TP_G17_Lowered_Idle"

tp("HandFlashlight", [
    ("Locomotion/FlashLight/A_Flashlight_CC", "A_TP_HandFlashlight_Idle"),
    ("Locomotion/FlashLight/A_RH_Flashlight_T2_Walk_Fwd", "A_TP_HandFlashlight_Walk_F"),
], LYRA)
# Same source as the G17's equip: its own copy so each can be tweaked
EXTRA_COPIES = {TP + "HandFlashlight/A_TP_HandFlashlight_Equip": LYRA + "Actions/MM_Pistol_Equip"}

for action, src in (("Idle", "Idle"), ("Equip", "Draw"), ("Fire", "Fire"), ("Reload", "Reload"), ("Reload_Empty", "Reload_Empty")):
    MOVE["/Game/Weapons/M4_T0/M4_Anim_" + src] = "/Game/Weapons/M4/Animations/A_FP_M4_" + action
for action, src in (("Idle", "Idle"), ("Equip", "Draw"), ("Fire", "Fire"), ("Reload", "Reload"), ("Reload_Empty", "ReloadEmpty")):
    MOVE["/Game/Weapons/G17/Glock17_Anim_" + src] = "/Game/Weapons/G17/Animations/A_FP_G17_" + action
for action, src in (("Idle", "AS_Flashlight_Idle"), ("Equip", "AS_Flashlight_Eqiup"), ("Inspect", "AS_Flashlight_Inspection")):
    COPY[FPF + src] = "/Game/Weapons/HandFlashlight/Animations/A_FP_HandFlashlight_" + action

SETS = "/Game/Characters/BodyAnimSets/"
SET_MOVES = {
    SETS + "DA_Body_Rifle": SETS + "DA_Body_M4",
    SETS + "DA_Body_Rifle_Lowered": SETS + "DA_Body_M4_Lowered",
    SETS + "DA_Body_Pistol": SETS + "DA_Body_G17",
    SETS + "DA_Body_Pistol_Lowered": SETS + "DA_Body_G17_Lowered",
    SETS + "DA_Body_Flashlight": SETS + "DA_Body_HandFlashlight",
}
BODY_FIELDS = ["idle", "walk_forward", "walk_backward", "walk_left", "walk_right", "crouch_idle", "crouch_forward",
               "crouch_backward", "crouch_left", "crouch_right", "falling", "upper_body_pose", "aim_offset", "fire",
               "reload", "equip", "hand_pose", "lowered_set"]
WEAPON_FIELDS = ["idle_animation", "equip_animation", "fire_animation", "reload_animation", "reload_empty_animation",
                 "third_person_pose", "body_anim_set"]
# (asset, field) -> path, where the general mapping isn't the answer
OVERRIDES = {(SETS + "DA_Body_HandFlashlight", "equip"): TP + "HandFlashlight/A_TP_HandFlashlight_Equip"}

MAPPING = dict(COPY)
MAPPING.update(MOVE)
MAPPING.update(SET_MOVES)


def out(msg):
    unreal.log_warning("[TH Anims] " + msg)


def package(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def copy(src, dst):
    if eal.does_asset_exist(dst):
        return eal.load_asset(dst)
    if not eal.does_asset_exist(src):
        raise RuntimeError("missing source " + src)
    if DRY:
        out("  would copy %s -> %s" % (src, dst))
        return None
    result = eal.duplicate_asset(src, dst)
    # duplicate_asset only creates the copy in memory: save it now, before anything is pointed at it
    if not result or not eal.save_asset(dst, False):
        raise RuntimeError("could not copy %s -> %s" % (src, dst))
    return result


def repoint(asset_path, fields):
    asset = eal.load_asset(asset_path)
    changed = []
    for field in fields:
        current = package(asset.get_editor_property(field))
        target = OVERRIDES.get((asset_path, field), MAPPING.get(current))
        if target and target != current:
            changed.append("%s: %s -> %s" % (field, current.split("/")[-1], target.split("/")[-1]))
            if not DRY:
                asset.set_editor_property(field, eal.load_asset(target))
    if changed and not DRY:
        eal.save_loaded_asset(asset, False)
    out("%s: %s" % (asset_path.split("/")[-1], "; ".join(changed) if changed else "nothing to change"))


def repoint_blend_space(path):
    space = eal.load_asset(path)
    samples = list(space.get_editor_property("sample_data"))
    moved = 0
    for sample in samples:
        current = package(sample.get_editor_property("animation"))
        target = MAPPING.get(current)
        if target:
            sample.set_editor_property("animation", eal.load_asset(target))
            moved += 1
    space.set_editor_property("sample_data", samples)
    eal.save_loaded_asset(space, False)
    left = [package(s.get_editor_property("animation")) for s in space.get_editor_property("sample_data")]
    stray = [p for p in left if p and not p.startswith(TP)]
    out("%s: %d samples repointed%s" % (path.split("/")[-1], moved, ("; still outside: " + ", ".join(stray)) if stray else ""))


def repoint_additive_bases():
    """Additive copies (aim offset poses) still name the pack's original as their base pose: use our copy."""
    for dst in list(COPY.values()) + list(MOVE.values()) + list(EXTRA_COPIES.keys()):
        anim = eal.load_asset(dst)
        if not isinstance(anim, unreal.AnimSequence):
            continue
        base = package(anim.get_editor_property("ref_pose_seq"))
        target = MAPPING.get(base)
        if target and target != base:
            if not DRY:
                anim.set_editor_property("ref_pose_seq", eal.load_asset(target))
                eal.save_loaded_asset(anim, False)
            out("%s: additive base %s -> %s" % (dst.split("/")[-1], base.split("/")[-1], target.split("/")[-1]))


if STEP == "bases":
    repoint_additive_bases()

elif STEP == "copy":
    out("%d copies, %d moves, %d body sets" % (len(COPY) + len(EXTRA_COPIES), len(MOVE), len(SET_MOVES)))
    for src, dst in list(COPY.items()) + list(MOVE.items()) + [(s, d) for d, s in EXTRA_COPIES.items()]:
        copy(src, dst)
    for src, dst in SET_MOVES.items():
        copy(src, dst)
    if not DRY:
        for dst in list(COPY.values()) + list(MOVE.values()) + list(EXTRA_COPIES.keys()):
            if dst.split("/")[-1].startswith("AO_"):
                repoint_blend_space(dst)
    for body_set in [SETS + "DA_Body_Unarmed"] + list(SET_MOVES.values()):
        repoint(body_set, BODY_FIELDS)
    for weapon in ("/Game/Weapons/Data/DA_M4", "/Game/Weapons/Data/DA_G17", "/Game/Weapons/Data/DA_HandFlashlight"):
        repoint(weapon, WEAPON_FIELDS)
    if not DRY:
        repoint_additive_bases()

elif STEP == "delete":
    doomed = list(MOVE.keys()) + list(SET_MOVES.keys())
    doomed_set = set(doomed)
    for path in doomed:
        if not eal.does_asset_exist(path):
            continue
        refs = sorted(set(str(r) for r in (reg.get_referencers(path, opts) or [])) - doomed_set)
        if refs:
            out("KEEP %s: still referred to by %s" % (path, ", ".join(refs)))
            continue
        out(("would delete " if DRY else "delete ") + path)
        if not DRY:
            eal.delete_asset(path)

else:
    out("set TH_STEP=copy or delete")
out("finished")

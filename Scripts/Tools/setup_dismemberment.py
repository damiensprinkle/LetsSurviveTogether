"""
Limbs that come off (developer, 2026-11-13: hits that matter, dismemberment). For each look in DA_ZombieLooks, the
Yarrawah Interactive pack's severed-limb meshes, one per body in the same order (they match that body's head, skin and
clothes), and the physics each falls with; BP_Zombie's blood spray and sound for a limb coming off (the flesh impact's,
from DA_ImpactSurfaces).

- M04 (eight bodies: 01-04 bald, 05-08 with hair): its Dismembered pieces (they carry the suit), falling with M02's limb
  physics (same skeleton; M04 has none of its own).
- M02 (two bodies): SK_Limb_*01 / *02 with their own physics.
- F02 (two bodies): SK_Limb_* (two heads) with their own physics.
- M09 Priest (one body; 2026-11-14): its Dismembered pieces, with M02's limb physics.
Both DA_ZombieLooks and DA_AmbusherLooks (whose Hazmat, Police and Construction looks have no pieces: they keep their limbs).

Re-run after changing the looks (setup_enemies.py rebuilds DA_ZombieLooks without these). Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_dismemberment.py" -nullrhi -unattended
"""
import unreal

YI = '/Game/ThirdParty/YI_ModularZombies/Meshes/'
# The zombies' looks and the ambushers' (the same plus Hazmat, Police and Construction, which have no pieces)
LOOKS_ASSETS = ['/Game/Enemies/Appearance/DA_ZombieLooks', '/Game/Enemies/Appearance/DA_AmbusherLooks']
ZOMBIE = '/Game/Enemies/BP_Zombie'
IMPACTS = '/Game/Effects/DA_ImpactSurfaces'
L = unreal.THLimb
M02_PHYS = {L.HEAD: 'PHYS_Limb_Head_M02', L.LEFT_ARM: 'PHYS_Limb_LeftArm_M02', L.RIGHT_ARM: 'PHYS_Limb_RightArm_M02',
            L.LEFT_LEG: 'PHYS_Limb_LeftLeg_M02', L.RIGHT_LEG: 'PHYS_Limb_RightLeg_M02'}
M04 = YI + 'ZombieM04/DismemberedLimbs/SK_Zombie_M04_Dismembered_'
M02 = YI + 'ZombieM02/DismemberedLimbs/SK_Limb_'
F02 = YI + 'ZombieF02/DismemberedLimbs/SK_Limb_'
M09 = YI + 'ZombieM09_Priest/DismemberedLimbs/SK_ZombieM09_Priest_Dismembered_'
# Look (by its first body's name) -> limb -> (meshes, one per body, wrapping; physics or None for the mesh's own)
SPEC = {
    'SK_Zombie_M04_01': {
        L.HEAD: ([M04 + 'HeadBald'] * 4 + [M04 + 'HeadHair'] * 4, M02_PHYS[L.HEAD]),
        L.LEFT_ARM: ([M04 + 'Arm_L'], M02_PHYS[L.LEFT_ARM]), L.RIGHT_ARM: ([M04 + 'Arm_R'], M02_PHYS[L.RIGHT_ARM]),
        L.LEFT_LEG: ([M04 + 'Leg_L'], M02_PHYS[L.LEFT_LEG]), L.RIGHT_LEG: ([M04 + 'Leg_R'], M02_PHYS[L.RIGHT_LEG]),
    },
    'SK_Zombie_M02_01': {
        L.HEAD: ([M02 + 'Head01_M02', M02 + 'Head02_M02'], None),
        L.LEFT_ARM: ([M02 + 'LeftArm01_M02', M02 + 'LeftArm02_M02'], None),
        L.RIGHT_ARM: ([M02 + 'RightArm01_M02', M02 + 'RightArm02_M02'], None),
        L.LEFT_LEG: ([M02 + 'LeftLeg01_M02', M02 + 'LeftLeg02_M02'], None),
        L.RIGHT_LEG: ([M02 + 'RightLeg01_M02', M02 + 'RightLeg02_M02'], None),
    },
    # The Priest (2026-11-14): its Dismembered pieces, falling with M02's limb physics (same bone names; none of its own)
    'SK_Priest': {
        L.HEAD: ([M09 + 'Head'], M02_PHYS[L.HEAD]),
        L.LEFT_ARM: ([M09 + 'Arm_L'], M02_PHYS[L.LEFT_ARM]), L.RIGHT_ARM: ([M09 + 'Arm_R'], M02_PHYS[L.RIGHT_ARM]),
        L.LEFT_LEG: ([M09 + 'Leg_L'], M02_PHYS[L.LEFT_LEG]), L.RIGHT_LEG: ([M09 + 'Leg_R'], M02_PHYS[L.RIGHT_LEG]),
    },
    'SK_Zombie_F02_01': {
        L.HEAD: ([F02 + 'Head_F02_01', F02 + 'Head_F02_02'], None),
        L.LEFT_ARM: ([F02 + 'ArmLeft_F02'], None), L.RIGHT_ARM: ([F02 + 'ArmRight_F02'], None),
        L.LEFT_LEG: ([F02 + 'LegLeft_F02'], None), L.RIGHT_LEG: ([F02 + 'LegRight_F02'], None),
    },
}
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH limbs] ' + str(msg))


def load(path):
    asset = eal.load_asset(path)
    if not asset:
        raise RuntimeError('missing ' + path)
    return asset


def limbs_for(LOOKS):
    looks = load(LOOKS)
    # Iterating gives copies of the looks: the edited copies are collected and written back
    entries = []
    for look in looks.get_editor_property('looks'):
        entries.append(look)
        bodies = look.get_editor_property('bodies')
        key = bodies[0].get_name() if bodies and bodies[0] else None
        spec = SPEC.get(key)
        if not spec:
            out('%s: no limbs' % key)
            continue
        limbs = []
        for limb, (meshes, physics) in spec.items():
            entry = unreal.THSeverableLimb()
            entry.set_editor_property('limb', limb)
            entry.set_editor_property('meshes', [load(m) for m in meshes])
            entry.set_editor_property('physics', load(YI + 'ZombieM02/DismemberedLimbs/' + physics) if physics else None)
            limbs.append(entry)
        look.set_editor_property('limbs', limbs)
        out('%s (%d bodies): %d limbs' % (key, len(bodies), len(limbs)))
    looks.set_editor_property('looks', entries)
    eal.save_loaded_asset(looks, False)


# The Priest's pieces came in with no materials (developer, 2026-11-15: limbs fell off looking wrong), and the pack's set
# for them (DismemberedLimbs/MI_ZombieM09_Priest_Dismember*) has every texture empty: they fell black (2026-11-16). They
# wear the body's own: skin, head, hair, and the cassock's upper and lower halves. Piece -> its slot -> material. The other
# looks' pieces take the body's materials at run time (ATHEnemyCharacter::MatchPieceMaterials)
M09_MATS = '/Game/ThirdParty/YI_ModularZombies/Materials/ZombieM09_Priest/'
SKIN, HEAD, HAIR = 'Body/MI_Zombie09_Priest_Body', 'Head/MI_Zombie09_Priest_Head', 'Hair/MI_Zombie09_Priest_Hair_01'
UPPER, LOWER = 'Clothing/MI_Zombie09_Priest_Body_Upper', 'Clothing/MI_Zombie09_Priest_Body_Lower'
PRIEST_PIECE_MATERIALS = {
    'Head': {'Body': HEAD, 'Hair': HAIR},
    'Arm_L': {'Body': SKIN, 'Upper': UPPER}, 'Arm_R': {'Body': SKIN, 'Upper': UPPER},
    'Leg_L': {'Body': SKIN, 'Lower': LOWER}, 'Leg_R': {'Body': SKIN, 'Lower': LOWER},
}


def dress_priest_pieces():
    for piece, slots in PRIEST_PIECE_MATERIALS.items():
        mesh = load(M09 + piece)
        # Iterating gives copies of the slots: the edited copies are collected and written back
        materials = []
        for each in mesh.get_editor_property('materials'):
            name = str(each.get_editor_property('material_slot_name'))
            if name in slots:
                each.set_editor_property('material_interface', load(M09_MATS + slots[name]))
            materials.append(each)
        mesh.set_editor_property('materials', materials)
        eal.save_loaded_asset(mesh, False)
        out('%s: %s' % (mesh.get_name(), ', '.join('%s=%s' % (str(m.get_editor_property('material_slot_name')),
                                                                 m.get_editor_property('material_interface').get_name() if m.get_editor_property('material_interface') else None)
                                                    for m in mesh.get_editor_property('materials'))))


def main():
    dress_priest_pieces()
    for path in LOOKS_ASSETS:
        out(path.split('/')[-1] + ':')
        limbs_for(path)

    # The flesh impact's blood spray and sounds, for a limb coming off
    impacts = load(IMPACTS)
    effect, sounds = None, []
    for surface, impact in impacts.get_editor_property('surfaces').items():
        candidate = impact.get_editor_property('effect')
        if candidate and 'blood' in candidate.get_path_name().lower():
            effect = candidate
            sounds = list(impact.get_editor_property('headshot_sounds')) or list(impact.get_editor_property('sounds'))
            break
    zombie = load(ZOMBIE)
    cdo = unreal.get_default_object(zombie.generated_class())
    cdo.set_editor_property('sever_effect', effect)
    # Gore sounds of their own once import_npc_audio.py has brought them in (developer, 2026-11-14); else the impact's
    own = eal.load_asset('/Game/Audio/Enemies/DismembermentBody') if eal.does_asset_exist('/Game/Audio/Enemies/DismembermentBody') else None
    cdo.set_editor_property('sever_sounds', [own] if own else sounds)
    eal.save_loaded_asset(zombie, False)
    out('BP_Zombie: sever effect %s, sounds %s' % (effect.get_name() if effect else None,
                                                   ', '.join(s.get_name() for s in cdo.get_editor_property('sever_sounds'))))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

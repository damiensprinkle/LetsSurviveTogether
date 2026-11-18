"""
The enemies (Milestone 4): zombies that hunt, stalk and ambush, a line-up of every body, and a flashlight, in L_EnemyTest.

1. /Game/Enemies/BP_Zombie: ATHEnemyCharacter with the Yarrawah Interactive zombie SK_Zombie_M04_01 (UE4 mannequin
   skeleton, its own full-body physics asset PHYS_ZombieM04), animated by UTHEnemyAnimInstance from DA_ZombieAnims.
   Health 200 (twice a player's), no downed state, Flesh bones (blood), ragdoll on death, ATHEnemyAIController: all
   from the C++ class. No behaviour on the Blueprint (it stands still); placed ones get one (step 3). Kill sound: High
   Impact Media's Guts cues. (Not M03: the pack's M03 meshes are a half body, torso and up.)
   Earlier runs made it as BP_Zombie_M03; this renames it and removes the redirector.
2. /Game/Enemies/Behaviors: DA_Hunter, DA_Stalker, DA_Ambusher (UTHEnemyBehavior: what an enemy does, and its
   tuning). Made with the numbers below only if missing: re-runs leave them alone, so tune them in the editor.
   /Game/Enemies/Animations: DA_ZombieAnims (standing) and DA_CrawlerAnims (the M03 half bodies), UTHEnemyAnimSet,
   from MocapOnline's in-place zombie clips (YI pack). Walk / run speeds are measured from each clip's root-motion
   twin (how far the pelvis travels); re-runs update these. The walk and run are the Zombie Animation Pack's anim_Walk_A and
   anim_Run_A (in place, no twins: measured from how fast their planted feet slide back).
   Voices: SA_EnemyVoice / SA_Voice (voice attenuations, /Game/Audio) and each behaviour's voice sounds from MaleCharVocLITE
   (hunter angry, ambusher scared, stalker gasping and moaning), only where a list is still empty.
3. Places three zombies in /Game/Maps/L_EnemyTest's hall, facing the doorway from the start room, each a different M04
   variant and behaviour: an ambusher by the crates near the door, a hunter in the middle, a stalker at the far end
   (folder TH_Enemies; re-running replaces those and nothing else). Their capsules don't change navigation.
4. Places the horror template's flashlight pickup (BP_FlashLight) in the start room, by the player starts
   (folder TH_Pickups; also replaced on re-run).
5. A line-up of one of every zombie and creature body we have, to check they all work (Functional.EnemyLineup):
   in the side room (south-west), three rows of seven facing its north doorway, with a light (folder TH_Lineup,
   tag TH_Lineup plus the body's name; replaced on re-run). Each is a BP_Zombie with its own mesh and that mesh's
   own physics asset and no behaviour (they stand still); all play DA_ZombieAnims (UE4 mannequin bone names,
   remapped by name onto the other skeletons), except the YI M03 half bodies, which play DA_CrawlerAnims.
     YI Modular Zombies (Yarrawah Interactive): M03 01-04 (half body), M04 01-08, M06 Hazmat, M07 Police,
       M08 Construction, M09 Priest
     Humanoid Creatures (Bugrimov Maksim, the mutated ones): Creature 01-05 (200 health too)

Run setup_impact_decals.py first (PM_Flesh and the blood). Re-running updates the Blueprint in place.
Creating the Blueprint needs Slate (not headless); once it exists, re-runs work headless:
  UnrealEditor-Cmd.exe "<abs>\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_enemies.py" -nullrhi -unattended
"""
import math
import unreal

FOLDER_PATH = '/Game/Enemies'
NAME = 'BP_Zombie'
OLD_NAMES = ['BP_Zombie_M03']      # renamed from these
MESHES = '/Game/ThirdParty/YI_ModularZombies/Meshes/ZombieM04/Zombie/SK_Zombie_M04_%02d'
MESH = MESHES % 1
PHYSICS = '/Game/ThirdParty/YI_ModularZombies/Meshes/ZombieM04/Zombie/PHYS_ZombieM04'
HEALTH = 200.0
# Played from the body when a shot (not a headshot) kills it: High Impact Media's Guts cues, faded like the impacts
KILL_SOUNDS = ['/Game/ThirdParty/BulletSFX_Flesh/Guts/CUE/No_Lead_in/Bullet_SFX_NoLeadIn_Guts_%02d_Cue' % i for i in range(1, 21)]
ATTENUATION = '/Game/Audio/SA_BulletImpact'   # made by setup_impact_decals.py

# Behaviours: (asset, mode, {property: value} over the C++ defaults). Only applied when the asset is first made.
BEHAVIOR_PATH = FOLDER_PATH + '/Behaviors'
BEHAVIORS = [
    ('DA_Hunter', 'HUNTER', {'run_speed': 330.0, 'attack_damage': 55.0, 'pace_radius': 250.0, 'reacts_to_light': True}),
    ('DA_Stalker', 'STALKER', {'run_speed': 300.0, 'retreat_speed': 330.0, 'attack_damage': 60.0, 'sight_half_angle': 80.0}),
    ('DA_Ambusher', 'AMBUSHER', {'run_speed': 380.0, 'attack_damage': 45.0, 'sight_half_angle': 90.0, 'wander_radius': 0.0, 'health': 90.0}),
    # A rare, heavily mutated enemy (developer, 2026-11-12): roars, charges (a hit kills), stumbles for a long while on a miss
    # Run speed 180, slower than a walking player (260): at 300 it kept up, so you could never open the distance it needs
    # to charge again (developer, 2026-11-17)
    ('DA_Charger', 'CHARGER', {'health': 400.0, 'run_speed': 180.0, 'attack_damage': 60.0, 'wander_radius': 400.0,
                               'charge_speed': 620.0, 'charge_damage': 100.0}),
    # Barely turned, still looks like a person (developer, 2026-11-12): lies calling for help, turns on whoever comes close
    ('DA_Siren', 'SIREN', {'health': 120.0, 'run_speed': 300.0, 'walk_speed': 140.0, 'attack_damage': 55.0, 'wander_radius': 0.0,
                         'idle_sound_interval': unreal.Vector2D(5.0, 10.0)}),
]

# Voices (developer's picks, 2026-11-11, from MaleCharVocLITE): the hunter angry, the ambusher scared, the stalker
# gasping (seen, and now and then) and moaning (right behind you). Set only where a list is still empty.
VOX = '/Game/ThirdParty/MaleCharVocLITE/Cues/'
ANGRY = ['Emotional_States/Angry/VOXMale_Angry_HA_MaleCharVoc_%s_Cue' % n for n in ('03', '08', '45', '49')]
SCARED = ['Emotional_States/Scared/VOXMale_Scared_HA_MaleCharVoc_%s_Cue' % n for n in ('06', '34', '38', '43')]
GASPING = ['Health_Conditions/Poisoned/Gasping/VOXMisc_Gasping_HA_MaleCharVoc_%s_Cue' % n for n in ('01', '04', '07')]
MOANING = ['Health_Conditions/Poisoned/Moaning/VOXMisc_Moaning_HA_MaleCharVoc_%s_Cue' % n for n in ('01', '09')]   # not 06 (developer, 2026-11-11)
# A hunter's calls while it paces (calling players to it; developer wants calls for help, but the pack has no words): hurt and scared
# Every cue in these folders
FOLDER_SETS = {
    'HUNTER_CALLS': ['Health_Conditions/Injured', 'Emotional_States/Scared'],
    # A siren's calls for help (no words in the pack: hurt, scared, sad) and its cry as it turns (combat attack shouts)
    'SIREN_CALLS': ['Health_Conditions/Injured', 'Emotional_States/Scared', 'Emotional_States/Sad'],
    'SIREN_ALERT': ['Combat/Attacks'],
}
# The charger: monster roars (as it charges) and growls (now and then), from MonsterRoarsAndGrowls
MONSTER = '/Game/ThirdParty/MonsterRoarsAndGrowls/cues/'
ROARS = [MONSTER + n for n in ('01_Primal_Roar_Cue', '02_Ferocious_Roar_Cue', '06_Battle_Roar_Cue', '10_Attack_Roar_Cue')]
GROWLS = [MONSTER + n for n in ('11_Menacing_Growl_Cue', '14_Guttural_Growl_Cue', '21_Lurking_Growl_Cue', '22_Searching_Growl_Cue')]
VOICES = {
    'DA_Hunter': {'alert_sounds': ANGRY, 'chase_sounds': ANGRY, 'idle_sounds': 'HUNTER_CALLS'},
    'DA_Ambusher': {'alert_sounds': SCARED, 'chase_sounds': SCARED},
    'DA_Stalker': {'spotted_sounds': GASPING, 'idle_sounds': GASPING, 'close_sounds': MOANING},
    'DA_Charger': {'alert_sounds': ROARS, 'idle_sounds': GROWLS},
    'DA_Siren': {'alert_sounds': 'SIREN_ALERT', 'chase_sounds': ANGRY, 'idle_sounds': 'SIREN_CALLS'},
}
# (path, inner radius, falloff): full volume within the inner radius, silent past inner + falloff
ENEMY_VOICE_ATTENUATION = '/Game/Audio/SA_EnemyVoice'
VOICE_ATTENUATIONS = [(ENEMY_VOICE_ATTENUATION, 300.0, 2700.0), ('/Game/Audio/SA_Voice', 200.0, 2300.0)]   # SA_Voice: players (ATHCharacter)

# Animation sets.MocapOnline's in-place clips (UE4 skeleton); each has a root-motion twin in ../Root_Motion.
ANIMS = '/Game/ThirdParty/YI_ModularZombies/Animation/MocapOnline/UE4/'
ANIM_PATH = FOLDER_PATH + '/Animations'
# QuokkaVoid's Zombie Animation Pack (UE4 mannequin copies): in place, with no root-motion twins. Its run replaced the YI
# hyper chase (developer, 2026-11-11: didn't like that run), then its walk replaced the YI slow chase to match.
ZAP = '/Game/ThirdParty/ZombieAnimationPack/Animations/Mannequin_UE4/'
YI_ANIMS = '/Game/ThirdParty/YI_ModularZombies/Animation/'
MIXAMO = '/Game/ThirdParty/Mixamo/Zombie/'   # import_mixamo.py's retargeted clips
LYRA_CROUCH = '/Game/ThirdParty/Lyra/Characters/Mannequin/Animations/Locomotion/Unarmed/MM_Unarmed_Crouch_Idle'
LYRA_CROUCH_WALK = '/Game/ThirdParty/Lyra/Characters/Mannequin/Animations/Locomotion/Unarmed/MM_Unarmed_Crouch_Walk_Fwd'
# Walk and run both from the Zombie Animation Pack, so they match (developer, 2026-11-11: the YI slow chase as the walk
# and this pack's run looked like two different zombies).
# Attacks are loops of several swings: one swing each, (clip, start, blow, end) in seconds. Start on the wind-up; the
# blow where the hands are fastest and furthest out (measured with THEditorScriptLibrary.get_bone_path).
ANIM_SETS = {
    'DA_ZombieAnims': {
        'idle': 'Zombie_Idle_1_v2', 'walk': ZAP + 'anim_Walk_A', 'run': ZAP + 'anim_Run_A',
        # A person's walk (a hunter pacing): the UE4 template walk that ships with the YI pack, on the zombie's own skeleton
        'calm_walk': YI_ANIMS + 'ThirdPerson/ThirdPersonWalk',
        'attacks': [('Zombie_Atk_Arms_3_SHORT_Loop', 0.45, 0.95, 1.35), ('Zombie_HyperAttack_1_SHORT_Loop', 0.5, 1.05, 1.3)],
        # A charger's roar (no roar clip in the packs: the reach, arms out) and its stumble: down, on the ground, back up
        'roar': ('Zombie_Reach_1_Loop', 0.0, 0.8, 1.6),
        'stumble_fall': ('Zombie_Stand_to_Crawl_1', 0.0, 1.0, 2.2), 'stumble_down': 'Zombie_Crawl_Idle_2_Loop',
        'stumble_get_up': ('Zombie_Crawl_2_to_Stand', 0.0, 4.0, 8.2),

        # Crouch: no zombie crouch in the packs, so Lyra's (UE5 mannequin, remapped by bone name; the arms, neck and head stay
        # the zombie's own). The stalker stays crouched throughout (developer, 2026-11-11).
        'crouch': LYRA_CROUCH, 'crouch_walk': LYRA_CROUCH_WALK,
        # Both arms shot off: Mixamo's Zombie Neck Bite (developer, 2026-11-16), to the head's lunge and back
        'armless_attacks': [(MIXAMO + 'ZM_Zombie_Neck_Bite_Anim_mixamo_com', 0.2, 1.05, 1.9)],
    },
    # The charger's own (developer, 2026-11-16: Mixamo's mutant clips, all of them). Walk and run aren't in place (Walk /
    # Run Travel hold them). Its stumble after a crash or a miss is a stun on its feet, not a fall (developer: "he shouldn't
    # fall over he should just be stunned"): a recoil, dizzy (Mixamo Dizzy Idle, 2026-11-16) for its Stumble Down Time,
    # then a short roar as it shakes it off. Roar: 2 s of the clip (rearing back, then the roar) in its 1.6 s Roar Time.
    'DA_MutantAnims': {
        'idle': MIXAMO + 'ZM_Mutant_Idle_Anim_mixamo_com', 'walk': MIXAMO + 'ZM_Mutant_Walking_Anim_mixamo_com',
        'run': MIXAMO + 'ZM_Mutant_Run_Anim_mixamo_com', 'calm_walk': MIXAMO + 'ZM_Mutant_Walking_Anim_mixamo_com',
        'attacks': [(MIXAMO + 'ZM_Mutant_Punch_Anim_mixamo_com', 0.0, 0.3, 1.1),
                    (MIXAMO + 'ZM_Mutant_Swiping_Anim_mixamo_com', 0.25, 1.0, 2.4)],
        'roar': (MIXAMO + 'ZM_Mutant_Roaring_Anim_mixamo_com', 0.3, 0.8, 2.3, 1.25),
        'stumble_fall': (MIXAMO + 'ZM_Zombie_Reaction_Hit_Anim_mixamo_com', 0.0, 0.5, 1.0),
        'stumble_down': MIXAMO + 'ZM_Dizzy_Idle_Anim_mixamo_com',
        'stumble_get_up': (MIXAMO + 'ZM_Mutant_Roaring_Anim_mixamo_com', 0.6, 1.0, 1.9),
        'behaviors': ['DA_Charger'],
    },
    # A person's (a siren; developer: only human-like animations on them): Mixamo clips retargeted onto the zombie skeleton
    # (import_mixamo.py) and the UE4 template walk and run that ship with the YI pack. Idle: injured, holding its side.
    # Attacks: three punches (blow where the fist is furthest out). Lying: the get-up from the back held on its first
    # frame (Mixamo's laying clips lie on a bed, ~95 cm up), then that get-up, from where it starts moving, sped up.
    'DA_HumanAnims': {
        'idle': MIXAMO + 'ZM_Injured_Idle', 'walk': YI_ANIMS + 'ThirdPerson/ThirdPersonWalk',
        'run': YI_ANIMS + 'ThirdPerson/ThirdPersonRun',
        'attacks': [(MIXAMO + 'ZM_Punching', 0.2, 0.62, 0.95), (MIXAMO + 'ZM_Punching_1', 0.1, 0.41, 0.75),
                    (MIXAMO + 'ZM_Punching_2', 0.15, 0.45, 0.9)],
        'stumble_down': MIXAMO + 'ZM_Getting_Up_Anim_mixamo_com',
        'stumble_get_up': (MIXAMO + 'ZM_Getting_Up_Anim_mixamo_com', 2.0, 4.0, 6.2, 1.5),
    },
    # The crawl: Mixamo's Zombie Crawl (developer, 2026-11-14: the YI crawl barely crept, 8 cm/s; this one goes about 80).
    # It isn't in place (its pelvis goes 2 m a loop): Walk Travel holds it in place (pelvis_travel)
    'DA_CrawlerAnims': {
        'idle': 'Zombie_Crawl_Idle_2_Loop', 'walk': MIXAMO + 'ZM_Zombie_Crawl_Anim_mixamo_com', 'run': None,
        'attacks': [('Zombie_Crawl_Attack_1_Loop', 3.5, 3.95, 4.3)],
    },
}

LEVEL = '/Game/Maps/L_EnemyTest'
OUTLINER_FOLDER = 'TH_Enemies'
TILE = 400.4                       # build_enemy_test_map.py's cell size
CAPSULE_HALF_HEIGHT = 90.0         # ATHEnemyCharacter
# Hall cells clear of the crates, the M04 variant, the behaviour
# Each kind in its own room (developer, 2026-11-12; build_enemy_test_map.py's layout), facing its door: (cell x, cell y,
# M04 variant, behaviour, face cell x, face cell y)
SPOTS = [(6.1, 1.0, 3, 'DA_Stalker', 3.0, 1.0),                       # the hall: crates and doorways to hide by
         (6.5, 5.6, 2, 'DA_Hunter', 6.0, 4.6),                        # its own room, to pace
         (10.2, 2.6, 4, 'DA_Charger', 7.2, 2.6),                      # the long charger hall: room to charge, and to sidestep
         (9.6, 5.8, 5, 'DA_Siren', 9.0, 4.6)]                          # its own lit room, lying there calling
# The ambusher pack, close together in the dark room
SPOTS += [(cx, cy, 1, 'DA_Ambusher', 3.0, 4.6) for cx, cy in ((3.6, 5.7), (4.2, 5.5), (4.4, 6.1), (3.8, 6.3))]
FACE = (3.0, 1.0)                  # the hall's doorway from the start room's corridor

FLASHLIGHT = '/Game/Template/Interaction/Blueprint/FlashLight/BP_FlashLight'
PICKUP_FOLDER = 'TH_Pickups'
FLASHLIGHT_SPOT = (0.6, 0.9, 10.0)  # start room, a step from the player starts (cell x, cell y, height)

YI = '/Game/ThirdParty/YI_ModularZombies/Meshes/'
YI_ROOT = '/Game/ThirdParty/YI_ModularZombies/'
APPEARANCE_PATH = FOLDER_PATH + '/Appearance'
CREATURES = '/Game/ThirdParty/Humanoid_Creatures/Mesh/'
# M03_02 to _04 have no physics asset of their own; _01's (upper body, like the meshes) fits them all
M03_PHYSICS = YI + 'ZombieM03/Zombie/PHYS_ZombieM03'
# (name, mesh, animation set or None for the Blueprint's, physics asset or None for the mesh's own), front row first,
# left to right from the doorway
LINEUP = (
    [('M03_%02d' % i, YI + 'ZombieM03/Zombie/SK_Zombie_M03_%02d' % i, 'DA_CrawlerAnims', M03_PHYSICS) for i in range(1, 5)]
    + [('M06_Hazmat', YI + 'ZombieM06_Hazmat/SK_Zombie_M06_Hazmat', None, None),
       ('M07_Police', YI + 'ZombieM07_Police/SK_ZombieM07_Police_01', None, None),
       ('M08_Construction', YI + 'ZombieM08_Construction/SK_ZombieM08_Construction', None, None)]
    + [('M04_%02d' % i, MESHES % i, None, None) for i in range(1, 9)]
    + [('M09_Priest', YI + 'ZombieM09_Priest/SK_Priest', None, None)]
    + [('Creature_%02d' % i, CREATURES + 'SK_Creature_%02d' % i, None, None) for i in range(1, 6)]
)
# Skeletons that took the YI idle's bone lengths (Functional.EnemyLineup measured: M08 collarbone 1.3x, Priest spine 3x):
# set to keep their own (bone translation retargeting), by THEditorScriptLibrary.keep_own_bone_lengths
KEEP_OWN_BONE_LENGTHS = [
    # The YI zombies' own skeleton too, so clips from other skeletons (the Zombie Animation Pack's mannequin, Lyra's UE5
    # crouch) keep the zombie's proportions (2026-11-11)
    YI.replace('Meshes/', 'Skeleton/') + 'UE4_Mannequin_Skeleton',
    YI.replace('Meshes/', 'Skeleton/') + 'SK_ZombieM08_Construction_Skeleton',
    YI + 'ZombieM09_Priest/SK_Priest_Skeleton',
]
LINEUP_FOLDER = 'TH_Lineup'
LINEUP_COLUMNS = [-0.3 + i * 1.6 / 6 for i in range(7)]   # side room x -0.5..1.5 (cells), about 1.07 m apart
LINEUP_ROWS = [4.1, 4.7, 5.3]                                # side room y 3.5..5.5; front row nearest the doorway
LINEUP_YAW = -90.0                                           # facing the north doorway (-Y)
LINEUP_LIGHT = (0.5, 4.5, 450.4)                             # the room's middle, at the ceiling
LIGHT_CLASS = '/Game/Environment/BP_OverheadLight'

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def out(msg):
    unreal.log_warning('[TH Enemies] ' + msg)


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('Missing ' + path)
    return asset


def save(asset, name):
    if not eal.save_loaded_asset(asset, False):
        raise RuntimeError('Could not save ' + name)


def data_asset(folder, name, cls):
    """The data asset at folder/name, made if missing. Returns (asset, made)."""
    path = folder + '/' + name
    if eal.does_asset_exist(path):
        return load(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    asset = tools.create_asset(name, folder, cls, factory)
    if not asset:
        raise RuntimeError('Could not create ' + path)
    return asset, True


def clip(name):
    """A YI clip by name (its in-place version), or any clip by full path."""
    return load(name if name.startswith('/Game/') else ANIMS + 'In_Place/' + name + '_IPC')


def travel_speed(name):
    """cm/s the clip moves across the ground at play rate 1."""
    if name.startswith('/Game/'):
        anim = load(name)
        # A clip that travels (root motion on its root, like Lyra's): how far its pelvis goes; an in-place one: its feet
        path = unreal.THEditorScriptLibrary.get_bone_path(anim, 'pelvis', 'None', 60)
        d = path[-1] - path[0]
        travelled = math.hypot(d.x, d.y) / anim.get_play_length()
        return travelled if travelled > 20.0 else planted_foot_speed(anim)
    # YI: how far the root-motion twin moves its pelvis
    twin = load(ANIMS + 'Root_Motion/' + name)
    path = unreal.THEditorScriptLibrary.get_bone_path(twin, 'pelvis', 'None', 60)
    d = path[-1] - path[0]
    return math.hypot(d.x, d.y) / twin.get_play_length()


def pelvis_travel(name):
    """How far a walk's pelvis goes in one loop under its root (a clip that isn't in place, like Mixamo's Zombie Crawl), in
    the mesh's space; zero for one in place (or with its motion on the root, which the animation code takes off)."""
    anim = load(name) if name.startswith('/Game/') else None
    if not anim:
        return unreal.Vector(0.0, 0.0, 0.0)
    lib = unreal.AnimationLibrary
    a = lib.get_bone_pose_for_time(anim, 'pelvis', 0.0, False).translation
    b = lib.get_bone_pose_for_time(anim, 'pelvis', anim.get_play_length() - 0.001, False).translation
    d = unreal.Vector(b.x - a.x, b.y - a.y, 0.0)
    return d if math.hypot(d.x, d.y) > 20.0 else unreal.Vector(0.0, 0.0, 0.0)


def planted_foot_speed(anim, steps=240):
    """For an in-place clip with no root-motion twin: how fast a planted foot slides back (the median speed of each
    ball bone while within 3 cm of its lowest), which is the speed the body would move over the ground. Checked on the
    YI hyper chase: 262 cm/s against 273 from its twin."""
    dt = anim.get_play_length() / steps
    speeds = []
    for bone in ('ball_l', 'ball_r'):
        path = unreal.THEditorScriptLibrary.get_bone_path(anim, bone, 'None', steps)
        low = min(p.z for p in path) + 3.0
        planted = sorted(math.hypot((b - a).x, (b - a).y) / dt
                         for a, b in zip(path, path[1:]) if a.z < low and b.z < low)
        if planted:
            speeds.append(planted[len(planted) // 2])
    return sum(speeds) / len(speeds) if speeds else 1.0


def make_behaviors():
    for name, mode, values in BEHAVIORS:
        asset, made = data_asset(BEHAVIOR_PATH, name, unreal.THEnemyBehavior)
        if not made:
            out('%s exists: left as tuned' % name)
            continue
        asset.set_editor_property('mode', getattr(unreal.THEnemyBehaviorMode, mode))
        for key, value in values.items():
            asset.set_editor_property(key, value)
        save(asset, name)
        out('%s made (%s)' % (name, mode.lower()))


def make_anim_sets():
    for name, spec in ANIM_SETS.items():
        asset, _ = data_asset(ANIM_PATH, name, unreal.THEnemyAnimSet)
        asset.set_editor_property('idle', clip(spec['idle']))
        asset.set_editor_property('walk', clip(spec['walk']))
        travel = pelvis_travel(spec['walk'])
        asset.set_editor_property('walk_travel', travel)
        moving = math.hypot(travel.x, travel.y) / clip(spec['walk']).get_play_length()
        asset.set_editor_property('walk_anim_speed', max(1.0, moving if moving > 0.0 else travel_speed(spec['walk'])))
        asset.set_editor_property('run', clip(spec['run']) if spec['run'] else None)
        asset.set_editor_property('calm_walk', clip(spec['calm_walk']) if spec.get('calm_walk') else None)
        if spec.get('calm_walk'):
            asset.set_editor_property('calm_walk_anim_speed', max(1.0, travel_speed(spec['calm_walk'])))
        asset.set_editor_property('crouch', clip(spec['crouch']) if spec.get('crouch') else None)
        asset.set_editor_property('crouch_walk', clip(spec['crouch_walk']) if spec.get('crouch_walk') else None)
        if spec.get('crouch_walk'):
            asset.set_editor_property('crouch_walk_anim_speed', max(1.0, travel_speed(spec['crouch_walk'])))
        if spec['run']:
            asset.set_editor_property('run_anim_speed', max(1.0, travel_speed(spec['run'])))
        asset.set_editor_property('run_travel', pelvis_travel(spec['run']) if spec['run'] else unreal.Vector(0.0, 0.0, 0.0))

        def attack_list(entries):
            attacks = []
            for anim, start, hit, end in entries:
                attack = unreal.THEnemyAttack()
                attack.set_editor_property('animation', clip(anim))
                attack.set_editor_property('start_time', start)
                attack.set_editor_property('hit_time', hit)
                attack.set_editor_property('end_time', end)
                attacks.append(attack)
            return attacks
        attacks = attack_list(spec['attacks'])
        asset.set_editor_property('attacks', attacks)
        asset.set_editor_property('armless_attacks', attack_list(spec.get('armless_attacks', [])))

        def window(spec_key):
            if not spec.get(spec_key):
                return unreal.THEnemyAttack()
            anim, start, hit, end = spec[spec_key][:4]
            w = unreal.THEnemyAttack()
            w.set_editor_property('animation', clip(anim))
            w.set_editor_property('start_time', start)
            w.set_editor_property('hit_time', hit)
            w.set_editor_property('end_time', end)
            if len(spec[spec_key]) > 4:
                w.set_editor_property('play_rate', spec[spec_key][4])
            return w
        asset.set_editor_property('roar', window('roar'))
        asset.set_editor_property('stumble_fall', window('stumble_fall'))
        asset.set_editor_property('stumble_down', clip(spec['stumble_down']) if spec.get('stumble_down') else None)
        asset.set_editor_property('stumble_get_up', window('stumble_get_up'))
        save(asset, name)
        # The behaviours that move with this set (the charger with the mutants')
        for behavior_name in spec.get('behaviors', []):
            behavior = load(BEHAVIOR_PATH + '/' + behavior_name)
            behavior.set_editor_property('animations', asset)
            save(behavior, behavior_name)
            out('%s animations: %s' % (behavior_name, name))
        out('%s: walk %.0f cm/s, run %.0f cm/s, %d attacks' % (name, asset.get_editor_property('walk_anim_speed'),
                                                             asset.get_editor_property('run_anim_speed'), len(attacks)))


def rename_old():
    """BP_Zombie_M03 -> BP_Zombie. Leaves a redirector until remove_redirectors() (after the zombies placed from it are replaced)."""
    new_path = FOLDER_PATH + '/' + NAME
    for old in OLD_NAMES:
        old_path = FOLDER_PATH + '/' + old
        if not eal.does_asset_exist(old_path) or eal.does_asset_exist(new_path):
            continue
        if not eal.rename_asset(old_path, new_path):
            raise RuntimeError('Could not rename %s to %s' % (old, NAME))
        out('renamed %s to %s' % (old, NAME))


def remove_redirectors():
    """The old names' redirectors, once nothing uses them (only the zombies placed in L_EnemyTest did)."""
    for old in OLD_NAMES:
        old_path = FOLDER_PATH + '/' + old
        if eal.does_asset_exist(old_path):
            if not eal.delete_asset(old_path):
                raise RuntimeError('Could not delete the redirector ' + old_path)
            out('removed the redirector ' + old)


def make_looks():
    """DA_ZombieLooks: random looks for BP_Zombie (developer, 2026-11-11: more variety in real levels). Yarrawah
    Interactive's three modular zombies with every clothing, colour, hair and gore variant they ship:
      M04 (suits): 8 bodies; body gore 2, head gore 3, suit 7 colours, hair 6
      M02 (Starved male): 2 bodies (two heads); body gore 4, each head 4; jeans 4 colours; T-shirt 6, singlet 6 or none;
        hair or none
      F02 (Starved female): 2 bodies (two heads); body 8, each head 5; cheerleader top and skirt (15, matching) or
        lingerie top (8); hair 2 colours or none
    Rebuilt on every run (it's data from the packs, not tuning)."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()

    def mats(folder, prefix):
        found = reg.get_assets_by_path(YI_ROOT + 'Materials/' + folder, recursive=True)
        names = sorted(str(a.package_name) for a in found
                       if str(a.asset_class_path.asset_name) == 'MaterialInstanceConstant' and str(a.asset_name).startswith(prefix))
        if not names:
            raise RuntimeError('No materials %s* in %s' % (prefix, folder))
        return [load(n) for n in names]

    def choice(slot, materials):
        c = unreal.THMaterialChoice()
        c.set_editor_property('slot', slot)
        c.set_editor_property('materials', materials)
        return c

    def outfit(meshes, materials):
        o = unreal.THOutfitChoice()
        o.set_editor_property('meshes', [load(YI + m) for m in meshes])
        o.set_editor_property('materials', materials)
        return o

    def part(name, choices):
        p = unreal.THOutfitPart()
        p.set_editor_property('name', name)
        p.set_editor_property('choices', choices)
        return p

    def look(name, bodies, physics, materials, parts, force_physics=False):
        body_meshes = [load(YI + b) for b in bodies]
        # The mesh's own physics asset, else the one given (M04's have none of their own)
        own = body_meshes[0].get_editor_property('physics_asset')
        l = unreal.THEnemyLook()
        l.set_editor_property('name', name)
        l.set_editor_property('bodies', body_meshes)
        l.set_editor_property('physics', load(physics) if force_physics or not own else None)
        l.set_editor_property('materials', materials)
        l.set_editor_property('parts', parts)
        out('look %s: %d bodies, physics %s, %d material slots, %d parts' % (name, len(bodies), physics.split('/')[-1] if force_physics or not own else own.get_name(),
                                                                               len(materials), len(parts)))
        return l

    looks = [
        look('M04', ['ZombieM04/Zombie/SK_Zombie_M04_%02d' % i for i in range(1, 9)], PHYSICS,
             [choice('Body', mats('ZombieM04/Body', 'MI_Body')), choice('Head', mats('ZombieM04/Head01', 'MI_Head01')),
              choice('Outfit', mats('ZombieM04/Clothing/Suit', 'MI_Clth_Suit')), choice('Hair', mats('ZombieM04/Hair', 'MI_Hair'))], []),
        look('M02', ['ZombieM02/Zombie/SK_Zombie_M02_01', 'ZombieM02/Zombie/SK_Zombie_M02_02'], YI + 'ZombieM02/Zombie/PHYS_ZombieM02',
             [choice('Body', mats('ZombieM02/Body', 'MI_Body')), choice('Head01', mats('ZombieM02/Head01', 'MI_Head01')),
              choice('Head02', mats('ZombieM02/Head02', 'MI_Head02'))],
             [part('Trousers', [outfit(['ZombieM02/Clothing/LongJeans/SK_Clth_LongPants_M02'], [choice('Jeans', mats('ZombieM02/Clothing/LongJeans', 'MI_Clth'))])]),
              part('Top', [outfit(['ZombieM02/Clothing/Tshirt/SK_Clth_Tshirt_M02'], [choice('Tshirt', mats('ZombieM02/Clothing/Tshirt', 'MI_Clth'))]),
                           outfit(['ZombieM02/Clothing/Singlet/SK_Clth_Singlet_M02'], [choice('Singlet', mats('ZombieM02/Clothing/Singlet', 'MI_Clth'))]),
                           outfit([], [])]),
              part('Hair', [outfit(['ZombieM02/Hair/SK_Hair_M02'], []), outfit([], [])])]),
        # F02's own physics asset (PHYS_Zombie_F02_NoBreasts) has collision off on the spine, upper arms and hair: shots went
        # through her torso (Functional.EnemyBehaviors). The Starved male's fits the same skeleton and a similar build.
        look('F02', ['ZombieF02/Zombie/SK_Zombie_F02_01', 'ZombieF02/Zombie/SK_Zombie_F02_02'], YI + 'ZombieM02/Zombie/PHYS_ZombieM02',
             [choice('Body', mats('ZombieF02/Body', 'MI_Body')), choice('Head01', mats('ZombieF02/Head01', 'MI_Head01')),
              choice('Head02', mats('ZombieF02/Head02', 'MI_Head02'))],
             [part('Outfit', [outfit(['ZombieF02/Clothing/OutfitCheerleader/SK_Clth_Singlet_Cheerleader_F02',
                                      'ZombieF02/Clothing/OutfitCheerleader/SK_Clth_Skirt_Cheerleader_F02'],
                                     [choice('Shirt', mats('ZombieF02/Clothing/OutfitCheerleader', 'MI_Clth'))]),
                              outfit(['ZombieF02/Clothing/OutfitLingerie/SK_Clth_Bra_Lingerie_F02'],
                                     [choice('Bra', mats('ZombieF02/Clothing/OutfitLingerie', 'MI_Clth'))])]),
              part('Hair', [outfit(['ZombieF02/Hair/SK_Hair_F02'], [choice('Hair', mats('ZombieF02/Hair', 'MI_Hair_')),
                                                                    choice('Haircap', mats('ZombieF02/Hair', 'MI_Haircap'))]),
                            outfit([], [])])], force_physics=True),
    ]
    # DA_HumanLooks (a siren): the same models with no gore and clean clothes only, the nearest thing to a person in the
    # packs until real human models arrive (their skin is still a zombie's)
    M = YI_ROOT + 'Materials/'

    def mi(*paths):
        return [load(M + p) for p in paths]
    human = [
        look('M04 clean', ['ZombieM04/Zombie/SK_Zombie_M04_%02d' % i for i in range(1, 9)], PHYSICS,
             [choice('Body', mi('ZombieM04/Body/Var01NoGore/MI_Body_Zombie_M04_01')),
              choice('Head', mi('ZombieM04/Head01/Var01_NoGore/MI_Head01_Zombie_M04_01')),
              choice('Outfit', mats('ZombieM04/Clothing/Suit', 'MI_Clth_Suit')), choice('Hair', mats('ZombieM04/Hair', 'MI_Hair'))], []),
        look('M02 clean', ['ZombieM02/Zombie/SK_Zombie_M02_01', 'ZombieM02/Zombie/SK_Zombie_M02_02'], YI + 'ZombieM02/Zombie/PHYS_ZombieM02',
             [choice('Body', mi('ZombieM02/Body/Var1NoGore/MI_Body_Zombie_M02_01')),
              choice('Head01', mi('ZombieM02/Head01/Var1NoGore/MI_Head01_Zombie_M02_01')),
              choice('Head02', mi('ZombieM02/Head02/Var1NoGore/MI_Head02_Zombie_M02_01'))],
             [part('Trousers', [outfit(['ZombieM02/Clothing/LongJeans/SK_Clth_LongPants_M02'],
                                       [choice('Jeans', mi('ZombieM02/Clothing/LongJeans/01BlueClean/MI_Clth_LongJeans_M02_01',
                                                           'ZombieM02/Clothing/LongJeans/03KhakiClean/MI_Clth_LongJeans_M02_03'))])]),
              part('Top', [outfit(['ZombieM02/Clothing/Tshirt/SK_Clth_Tshirt_M02'], [choice('Tshirt', mi('ZombieM02/Clothing/Tshirt/01WhiteClean/MI_Clth_Tshirts_M02_01'))]),
                           outfit(['ZombieM02/Clothing/Singlet/SK_Clth_Singlet_M02'],
                                  [choice('Singlet', mi('ZombieM02/Clothing/Singlet/01WhiteClean/MI_Clth_Singlet_M02_01',
                                                        'ZombieM02/Clothing/Singlet/05SafetyVest/MI_Clth_Singlet_M02_05',
                                                        'ZombieM02/Clothing/Singlet/06RedWhiteStyled/MI_Clth_Singlet_M02_06'))])]),
              part('Hair', [outfit(['ZombieM02/Hair/SK_Hair_M02'], []), outfit([], [])])]),
        look('F02 clean', ['ZombieF02/Zombie/SK_Zombie_F02_01', 'ZombieF02/Zombie/SK_Zombie_F02_02'], YI + 'ZombieM02/Zombie/PHYS_ZombieM02',
             [choice('Body', mi('ZombieF02/Body/01NoGore/MI_Body_Zombie_F02_01')),
              choice('Head01', mi('ZombieF02/Head01/01NoGore/MI_Head01_Zombie_F02_01')),
              choice('Head02', mi('ZombieF02/Head02/01NoGore/MI_Head02_Zombie_F02_01'))],
             [part('Outfit', [outfit(['ZombieF02/Clothing/OutfitCheerleader/SK_Clth_Singlet_Cheerleader_F02',
                                      'ZombieF02/Clothing/OutfitCheerleader/SK_Clth_Skirt_Cheerleader_F02'],
                                     [choice('Shirt', mi('ZombieF02/Clothing/OutfitCheerleader/01CleanRed/MI_Clth_CheerleaderOutfit_F02_01',
                                                         'ZombieF02/Clothing/OutfitCheerleader/02CleanBlue/MI_Clth_CheerleaderOutfit_F02_02',
                                                         'ZombieF02/Clothing/OutfitCheerleader/03CleanGreen/MI_Clth_CheerleaderOutfit_F02_03'))])]),
              part('Hair', [outfit(['ZombieF02/Hair/SK_Hair_F02'], [choice('Hair', mi('ZombieF02/Hair/01Blonde/MI_Hair_F02_01')),
                                                                    choice('Haircap', mi('ZombieF02/Hair/01Blonde/MI_Haircap_F02_01'))])])],
             force_physics=True),
    ]
    humans, _ = data_asset(APPEARANCE_PATH, 'DA_HumanLooks', unreal.THEnemyAppearance)
    humans.set_editor_property('looks', human)
    save(humans, 'DA_HumanLooks')
    siren = load(BEHAVIOR_PATH + '/DA_Siren')
    changed = False
    if not siren.get_editor_property('appearance'):
        siren.set_editor_property('appearance', humans)
        changed = True
    if not siren.get_editor_property('animations'):
        siren.set_editor_property('animations', load(ANIM_PATH + '/DA_HumanAnims'))
        changed = True
    if changed:
        save(siren, 'DA_Siren')

    # More variety (developer, 2026-11-14). The Priest (M09) has its own severed limbs: a zombie like the rest (one body,
    # its own materials and physics; half as likely as the others). Hazmat (M06), Police (M07) and Construction (M08)
    # have no limb pieces, so they can't lose limbs: only ambushers wear them (DA_AmbusherLooks: those three and every
    # zombie look; DA_Ambusher's Appearance, setup_tactics.py). One body each, as shipped (no colour variants).
    def single(name, mesh, weight):
        l = look(name, [mesh], PHYSICS, [], [])
        l.set_editor_property('weight', weight)
        return l
    looks.append(single('M09 Priest', 'ZombieM09_Priest/SK_Priest', 0.5))
    asset, _ = data_asset(APPEARANCE_PATH, 'DA_ZombieLooks', unreal.THEnemyAppearance)
    asset.set_editor_property('looks', looks)
    save(asset, 'DA_ZombieLooks')
    ambushers, _ = data_asset(APPEARANCE_PATH, 'DA_AmbusherLooks', unreal.THEnemyAppearance)
    ambushers.set_editor_property('looks', looks + [single('M06 Hazmat', 'ZombieM06_Hazmat/SK_Zombie_M06_Hazmat', 0.6),
                                                     single('M07 Police', 'ZombieM07_Police/SK_ZombieM07_Police_01', 0.6),
                                                     single('M08 Construction', 'ZombieM08_Construction/SK_ZombieM08_Construction', 0.6)])
    save(ambushers, 'DA_AmbusherLooks')
    return asset


def make_charger():
    """DA_MutantLooks (Bugrimov Maksim's five Humanoid Creatures, each its own physics asset) as the charger's own
    looks, so a charger is always a mutant whatever enemy it's put on."""
    looks = unreal.THEnemyLook()
    looks.set_editor_property('name', 'Mutants')
    looks.set_editor_property('bodies', [load(CREATURES + 'SK_Creature_%02d' % i) for i in range(1, 6)])
    asset, _ = data_asset(APPEARANCE_PATH, 'DA_MutantLooks', unreal.THEnemyAppearance)
    asset.set_editor_property('looks', [looks])
    save(asset, 'DA_MutantLooks')
    charger = load(BEHAVIOR_PATH + '/DA_Charger')
    if not charger.get_editor_property('appearance'):
        charger.set_editor_property('appearance', asset)
        save(charger, 'DA_Charger')
    out('DA_MutantLooks: 5 mutant bodies; DA_Charger looks %s' % charger.get_editor_property('appearance').get_name())


def make_attenuation(path, inner, falloff):
    asset = unreal.load_asset(path)
    if not asset:
        name = path.split('/')[-1]
        asset = tools.create_asset(name, path.rsplit('/', 1)[0], unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = asset.get_editor_property('attenuation')
    settings.set_editor_property('attenuate', True)
    settings.set_editor_property('spatialize', True)
    settings.set_editor_property('attenuation_shape', unreal.AttenuationShape.SPHERE)
    settings.set_editor_property('attenuation_shape_extents', unreal.Vector(inner, 0, 0))
    settings.set_editor_property('falloff_distance', falloff)
    asset.set_editor_property('attenuation', settings)
    save(asset, path.split('/')[-1])
    return asset


def make_voices():
    """Attenuations for voices, and each behaviour's voice sounds (only lists still empty: re-runs keep your changes)."""
    for path, inner, falloff in VOICE_ATTENUATIONS:
        make_attenuation(path, inner, falloff)
    enemy_attenuation = load(ENEMY_VOICE_ATTENUATION)
    for name, lists in VOICES.items():
        asset = load(BEHAVIOR_PATH + '/' + name)
        changed = []
        for key, sounds in lists.items():
            if not asset.get_editor_property(key):
                if isinstance(sounds, str):
                    reg = unreal.AssetRegistryHelpers.get_asset_registry()
                    paths = sorted(str(a.package_name) for folder in FOLDER_SETS[sounds] for a in reg.get_assets_by_path(VOX + folder, recursive=True))
                else:
                    paths = [s if s.startswith('/Game/') else VOX + s for s in sounds]
                asset.set_editor_property(key, [load(p) for p in paths])
                changed.append(key)
        if not asset.get_editor_property('voice_attenuation'):
            asset.set_editor_property('voice_attenuation', enemy_attenuation)
            changed.append('voice_attenuation')
        if changed:
            save(asset, name)
        out('%s voice: %s' % (name, ', '.join(changed) if changed else 'left as set'))


import sys
try:
    # Only the animation sets (no level work): -script="<abs>/Scripts/Tools/setup_enemies.py anim_sets_only"
    if 'anim_sets_only' in sys.argv:
        make_anim_sets()
        raise RuntimeError('anim sets only: done (the rest skipped)')
    # Only the looks (DA_ZombieLooks, DA_AmbusherLooks, DA_HumanLooks); then re-run setup_dismemberment.py
    if 'looks_only' in sys.argv:
        make_looks()
        raise RuntimeError('looks only: done (the rest skipped); now run setup_dismemberment.py')
    make_behaviors()
    make_anim_sets()
    make_voices()
    looks = make_looks()
    make_charger()

    rename_old()
    bp = unreal.load_asset(FOLDER_PATH + '/' + NAME)
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.THEnemyCharacter)
        bp = tools.create_asset(NAME, FOLDER_PATH, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('body_mesh', load(MESH))
    cdo.set_editor_property('animations', load(ANIM_PATH + '/DA_ZombieAnims'))
    cdo.set_editor_property('behavior', None)
    cdo.set_editor_property('physics_asset_override', load(PHYSICS))
    # Random looks for every BP_Zombie (the line-up below sets none, to keep its bodies)
    cdo.set_editor_property('appearance', looks)
    cdo.set_editor_property('kill_sounds', [load(p) for p in KILL_SOUNDS])
    cdo.set_editor_property('kill_sound_attenuation', load(ATTENUATION))
    cdo.set_editor_property('ai_controller_class', unreal.THEnemyAIController.static_class())
    cdo.get_editor_property('health_component').set_editor_property('max_health', HEALTH)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp, NAME)
    out('%s: body %s, health %.0f' % (NAME, MESH.split('/')[-1], HEALTH))

    for path in KEEP_OWN_BONE_LENGTHS:
        skeleton = load(path)
        changed = unreal.THEditorScriptLibrary.keep_own_bone_lengths(skeleton)
        if changed:
            save(skeleton, path)
        out('%s: %d bones set to keep their own length' % (path.split('/')[-1], changed))

    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    for actor in actors.get_all_level_actors():
        if str(actor.get_folder_path()) in (OUTLINER_FOLDER, PICKUP_FOLDER, LINEUP_FOLDER):
            actors.destroy_actor(actor)
    zombie_class = bp.generated_class()
    for i, (cx, cy, variant, behavior, fx, fy) in enumerate(SPOTS):
        yaw = math.degrees(math.atan2(fy - cy, fx - cx))
        location = unreal.Vector(cx * TILE, cy * TILE, CAPSULE_HALF_HEIGHT + 2.0)
        zombie = actors.spawn_actor_from_class(zombie_class, location, unreal.Rotator(0, 0, yaw))
        zombie.set_actor_label('TH_Zombie%d_%s' % (i + 1, behavior[3:]))
        zombie.set_folder_path(OUTLINER_FOLDER)
        zombie.set_editor_property('behavior', load(BEHAVIOR_PATH + '/' + behavior))
        if variant != 1:
            zombie.set_editor_property('body_mesh', load(MESHES % variant))
        out('TH_Zombie%d (M04_%02d, %s) at %s facing yaw %.0f' % (i + 1, variant, behavior, location, yaw))

    fx, fy, fz = FLASHLIGHT_SPOT
    light = actors.spawn_actor_from_class(load(FLASHLIGHT).generated_class(), unreal.Vector(fx * TILE, fy * TILE, fz), unreal.Rotator(0, 0, 90))
    light.set_actor_label('TH_FlashLight')
    light.set_folder_path(PICKUP_FOLDER)
    out('TH_FlashLight at %s' % light.get_actor_location())

    for i, (name, mesh, anims, physics) in enumerate(LINEUP):
        cx = LINEUP_COLUMNS[i % len(LINEUP_COLUMNS)]
        cy = LINEUP_ROWS[i // len(LINEUP_COLUMNS)]
        body = actors.spawn_actor_from_class(zombie_class, unreal.Vector(cx * TILE, cy * TILE, CAPSULE_HALF_HEIGHT + 2.0), unreal.Rotator(0, 0, LINEUP_YAW))
        body.set_actor_label('TH_Lineup_' + name)
        body.set_folder_path(LINEUP_FOLDER)
        body.tags = [unreal.Name('TH_Lineup'), unreal.Name(name)]
        body.set_editor_property('physics_asset_override', load(physics) if physics else None)
        body.set_editor_property('body_mesh', load(mesh))
        body.set_editor_property('appearance', None)
        if anims:
            body.set_editor_property('animations', load(ANIM_PATH + '/' + anims))
    lx, ly, lz = LINEUP_LIGHT
    lamp = actors.spawn_actor_from_class(eal.load_blueprint_class(LIGHT_CLASS), unreal.Vector(lx * TILE, ly * TILE, lz), unreal.Rotator(0, 0, 0))
    lamp.set_actor_label('TH_Lineup_Light')
    lamp.set_folder_path(LINEUP_FOLDER)
    out('line-up: %d bodies in the side room' % len(LINEUP))

    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError('Could not save ' + LEVEL)
    remove_redirectors()
    out('done')
except Exception as error:
    out('FAILED: %s' % error)
finally:
    unreal.SystemLibrary.quit_editor()

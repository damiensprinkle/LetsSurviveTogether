"""
How a siren waits (DA_HumanAnims' Lying Poses), one at random each time (developer, 2026-11-14: a siren never lies there
waiting; it stands or sits). From the Mixamo clips retargeted by import_mixamo.py (/Game/ThirdParty/Mixamo/Zombie):

- Standing, terrified: Terrified looped; a lone player near, it waves them over (Injured Wave Idle). Up: no get-up clip,
  the pose eases into its moves (half a second).
- Sitting on the floor, dazed: Sitting Dazed looped (no wave clip sitting). Up: eases out the same way.
- Playing dead only (shot down to half its health it drops): on its back, Laying Sleeping (recorded on a bed: lowered
  96 cm), up with Getting Up 2.0-6.2 s at 2.5x (about 1.7 s). Never picked for waiting.
(Until 2026-11-14 it lay on its back or its stomach, waving from its back with Laying Hand Gesture.)

Re-run after changing the picks below. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_siren_lying.py" -nullrhi -unattended
"""
import unreal

ANIMS = '/Game/Enemies/Animations/DA_HumanAnims'
CLIPS = '/Game/ThirdParty/Mixamo/Zombie/'
BED = 96.0
POSES = [
    # get-up clip (None: eases out), start, end, rate, waiting clip, beckoning clip, lowered (waiting and beckoning),
    # playing dead only
    (None, 0.0, 0.0, 1.0, 'ZM_Terrified_Anim_mixamo_com', 'ZM_Injured_Wave_Idle', 0.0, False),
    (None, 0.0, 0.0, 1.0, 'ZM_Sitting_Dazed_Anim_mixamo_com', None, 0.0, False),
    ('ZM_Getting_Up_Anim_mixamo_com', 2.0, 6.2, 2.5, 'ZM_Laying_Sleeping', None, BED, True),
]
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH siren] ' + str(msg))


def clip(name):
    if not name:
        return None
    asset = eal.load_asset(CLIPS + name)
    if not asset:
        raise RuntimeError('missing ' + CLIPS + name)
    return asset


def main():
    anims = eal.load_asset(ANIMS)
    poses = []
    for get_up, start, end, rate, lying, beckon, lower, dead_only in POSES:
        attack = unreal.THEnemyAttack()
        attack.set_editor_property('animation', clip(get_up))
        attack.set_editor_property('start_time', start)
        attack.set_editor_property('end_time', end)
        attack.set_editor_property('play_rate', rate)
        pose = unreal.THEnemyLyingPose()
        pose.set_editor_property('get_up', attack)
        pose.set_editor_property('lying', clip(lying))
        pose.set_editor_property('reacting', clip(beckon))
        pose.set_editor_property('lying_lower_by', lower if lying else 0.0)
        pose.set_editor_property('reacting_lower_by', lower if beckon else 0.0)
        pose.set_editor_property('playing_dead_only', dead_only)
        poses.append(pose)
        out('%s, up with %s, beckoning %s, lowered %.0f%s' % (lying, get_up or '(eases out)', beckon, lower, ', playing dead only' if dead_only else ''))
    anims.set_editor_property('lying_poses', poses)
    eal.save_loaded_asset(anims, False)
    out('DA_HumanAnims lying poses: %d' % len(anims.get_editor_property('lying_poses')))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

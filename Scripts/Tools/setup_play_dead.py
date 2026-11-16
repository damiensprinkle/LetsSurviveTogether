"""
Zombies playing dead (developer, 2026-11-13: a pack of "dead" ambushers that all jump up). DA_ZombieAnims' Lying Poses,
one at random each time a zombie lies down (an ambusher waiting, Lies Down on its behaviour):

- Face down: Mixamo's Stand Up (import_mixamo.py) held on its first frame, then the whole clip at 1.3x: about 0.9 s
  (developer, 2026-11-14; until then on the back with the human get-up sped up, or face down with the YI crawl-to-stand).

Re-run after changing the picks. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_play_dead.py" -nullrhi -unattended
"""
import unreal

ANIMS = '/Game/Enemies/Animations/DA_ZombieAnims'
YI = '/Game/ThirdParty/YI_ModularZombies/Animation/MocapOnline/UE4/In_Place/'
MIXAMO = '/Game/ThirdParty/Mixamo/Zombie/'
POSES = [
    # (lying clip, hold at s or None, get-up clip, start, end, rate)
    # Face down, then Mixamo's Stand Up (developer, 2026-11-14: a zombie springing up from face down, for the ambushers;
    # it replaces the human get-up from the back sped up 3x and the YI crawl-to-stand): held on its first frame, then the
    # whole clip (1.2 s) at 1.3x, about 0.9 s
    (None, None, MIXAMO + 'ZM_Stand_Up_Anim_mixamo_com', 0.0, 1.17, 1.3),
]
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH dead] ' + str(msg))


def load(path):
    asset = eal.load_asset(path)
    if not asset:
        raise RuntimeError('missing ' + path)
    return asset


def main():
    poses = []
    for lying, hold, get_up, start, end, rate in POSES:
        attack = unreal.THEnemyAttack()
        attack.set_editor_property('animation', load(get_up))
        attack.set_editor_property('start_time', start)
        attack.set_editor_property('end_time', end)
        attack.set_editor_property('play_rate', rate)
        pose = unreal.THEnemyLyingPose()
        pose.set_editor_property('get_up', attack)
        if lying:
            clip = load(lying)
            pose.set_editor_property('lying', clip)
            pose.set_editor_property('lying_hold_time', max(clip.get_play_length() - 0.02, 0.0) if hold == 'end' else -1.0)
        poses.append(pose)
        out('%s held %s, up with %s %.1f-%.1f s at %.1fx' % (lying.split('/')[-1] if lying else '(get-up start)', hold, get_up.split('/')[-1], start, end, rate))
    anims = load(ANIMS)
    anims.set_editor_property('lying_poses', poses)
    eal.save_loaded_asset(anims, False)
    out('DA_ZombieAnims: %d lying poses' % len(anims.get_editor_property('lying_poses')))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

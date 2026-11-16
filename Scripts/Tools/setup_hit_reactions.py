"""
Enemies stagger when shot (developer, 2026-11-13: the hunter, stalker and siren should be slightly staggered when shot;
then: hits that matter, by where they came from). Lyra's hit reacts (UE5 mannequin, played on the zombie skeleton by
bone name like Lyra's crouch; they were in _Archive) are copied into /Game/ThirdParty/Lyra/Characters/Mannequin/
Animations/Actions and set as the Hit Reactions of the zombie and human body sets, each tagged with the side it reacts
to and whether it is heavy (a headshot or big hit: the medium and heavy ones); since 2026-11-14 only the siren's (DA_HumanAnims):
the zombies use two Mixamo zombie hits instead. The zombie set crawls with
DA_CrawlerAnims when a leg is shot off (Legs Lost). DA_Hunter, DA_Stalker and DA_Siren get a Stagger Time.

Re-run after changing the picks. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_hit_reactions.py" -nullrhi -unattended
"""
import unreal

ARCHIVE = '/Game/_Archive/Animations/Lyra/Characters/Mannequin/Animations/Actions/'
DEST = '/Game/ThirdParty/Lyra/Characters/Mannequin/Animations/Actions/'
CROUCH = '/Game/ThirdParty/Lyra/Characters/Mannequin/Animations/Locomotion/Unarmed/MM_Unarmed_Crouch_Idle'
S = unreal.THHitSide
# (clip, side, heavy): every Lyra hit react; light = Lgt, heavy = Med and Hvy
CLIPS = [
    ('MM_HitReact_Front_Lgt_01', S.FRONT, False), ('MM_HitReact_Front_Lgt_02', S.FRONT, False),
    ('MM_HitReact_Front_Lgt_03', S.FRONT, False), ('MM_HitReact_Front_Lgt_04', S.FRONT, False),
    ('MM_HitReact_Front_Med_01', S.FRONT, True), ('MM_HitReact_Front_Med_02', S.FRONT, True),
    ('MM_HitReact_Front_Hvy_01', S.FRONT, True),
    ('MM_HitReact_Back_Lgt_01', S.BACK, False), ('MM_HitReact_Back_Med_01', S.BACK, True),
    ('MM_HitReact_Left_Lgt_01', S.LEFT, False), ('MM_HitReact_Left_Med_01', S.LEFT, True),
    ('MM_HitReact_Right_Lgt_01', S.RIGHT, False), ('MM_HitReact_Right_Med_01', S.RIGHT, True),
]
PLAY_RATE = 1.2
# Zombies flinch like zombies (developer, 2026-11-14: Lyra's soldier flinches looked human; two Mixamo zombie hits,
# import_mixamo.py). Whole-body clips, so only their start: (clip, start, end, rate, heavy, side). Reaction Hit stays put;
# Reaction Hit (1) is knocked back 70 cm after 0.9 s, so it stops before that. Both from the front; from behind (developer,
# 2026-11-16): Hit Reaction_Behind, buckling forward and back up (it doesn't move). Side hits play any of them.
MIXAMO = '/Game/ThirdParty/Mixamo/Zombie/'
ZOMBIE_CLIPS = [('ZM_Zombie_Reaction_Hit_Anim_mixamo_com', 0.0, 1.0, 1.3, False, S.FRONT),
                ('ZM_Zombie_Reaction_Hit_1_Anim_mixamo_com', 0.0, 0.85, 1.2, True, S.FRONT),
                ('ZM_Hit_Reaction_Behind_Anim_mixamo_com', 0.0, 1.5, 1.3, False, S.BACK)]
ZOMBIE_SET = '/Game/Enemies/Animations/DA_ZombieAnims'
SETS = ['/Game/Enemies/Animations/DA_HumanAnims']
LEGS_LOST = {'/Game/Enemies/Animations/DA_ZombieAnims': '/Game/Enemies/Animations/DA_CrawlerAnims'}
STAGGER = {'/Game/Enemies/Behaviors/DA_Hunter': 0.4, '/Game/Enemies/Behaviors/DA_Stalker': 0.4, '/Game/Enemies/Behaviors/DA_Siren': 0.5}
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH hits] ' + str(msg))


def main():
    crouch = eal.load_asset(CROUCH)
    skeleton = crouch.get_editor_property('skeleton') if crouch else None
    reactions = []
    for name, side, heavy in CLIPS:
        if not eal.does_asset_exist(DEST + name):
            eal.duplicate_asset(ARCHIVE + name, DEST + name)
        clip = eal.load_asset(DEST + name)
        if not clip:
            out('MISSING ' + name)
            continue
        if clip.get_editor_property('skeleton') != skeleton:
            out('%s: NOT on the crouch\'s skeleton' % name)
        eal.save_loaded_asset(clip, False)
        attack = unreal.THEnemyAttack()
        attack.set_editor_property('animation', clip)
        attack.set_editor_property('play_rate', PLAY_RATE)
        reaction = unreal.THEnemyHitReaction()
        reaction.set_editor_property('clip', attack)
        reaction.set_editor_property('side', side)
        reaction.set_editor_property('heavy', heavy)
        reactions.append(reaction)
        out('%s: %.2f s, %s%s' % (name, clip.get_play_length(), side, ', heavy' if heavy else ''))
    zombie_reactions = []
    for name, start, end, rate, heavy, side in ZOMBIE_CLIPS:
        attack = unreal.THEnemyAttack()
        attack.set_editor_property('animation', eal.load_asset(MIXAMO + name))
        attack.set_editor_property('start_time', start)
        attack.set_editor_property('end_time', end)
        attack.set_editor_property('play_rate', rate)
        reaction = unreal.THEnemyHitReaction()
        reaction.set_editor_property('clip', attack)
        reaction.set_editor_property('side', side)
        reaction.set_editor_property('heavy', heavy)
        zombie_reactions.append(reaction)
        out('%s: %.2f-%.2f s at %.1fx%s' % (name, start, end, rate, ', heavy' if heavy else ''))
    for path in SETS + [ZOMBIE_SET]:
        anims = eal.load_asset(path)
        anims.set_editor_property('hit_reactions', zombie_reactions if path == ZOMBIE_SET else reactions)
        if path in LEGS_LOST:
            anims.set_editor_property('legs_lost', eal.load_asset(LEGS_LOST[path]))
        eal.save_loaded_asset(anims, False)
        out('%s: %d hit reactions, legs lost %s' % (path.split('/')[-1], len(anims.get_editor_property('hit_reactions')), anims.get_editor_property('legs_lost')))
    for path, seconds in STAGGER.items():
        behavior = eal.load_asset(path)
        behavior.set_editor_property('stagger_time', seconds)
        eal.save_loaded_asset(behavior, False)
        out('%s: stagger %.1f s' % (path.split('/')[-1], seconds))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

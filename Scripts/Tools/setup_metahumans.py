"""
MetaHumans as the siren's looks (developer, 2026-11-12: real people; made in the MetaHuman Creator, assembled Optimized,
Medium, Joints Only, into /Game/MetaHumans/<Name>).

1. An IK rig for the MetaHuman body skeleton (auto retarget chains), and a retargeter from the zombie skeleton the enemy
   animations are on (IKRig_Zombie, import_mixamo.py) onto it: /Game/MetaHumans/Retarget.
2. DA_MetaHumanLooks (/Game/Enemies/Appearance): one look per MetaHuman found in /Game/MetaHumans: its body, its face
   and outfit following the body, its grooms (hair, eyebrows) on the face, and the retargeter. The animations play on a
   hidden zombie body (DA_HumanLooks' first body and physics), which also takes the shots and falls as the ragdoll;
   ATHEnemyCharacter copies its pose onto the MetaHuman every frame.
3. DA_Siren's Appearance becomes DA_MetaHumanLooks (DA_HumanLooks is kept).

Re-run after adding or re-assembling a MetaHuman, then setup_siren_blood.py (the looks are rebuilt without the blood on
their faces; Content.SirenBlood fails until then). Needs the editor (offscreen) for the IK rig and retargeter:
  UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>/Scripts/Tools/setup_metahumans.py" -RenderOffScreen -unattended -nosound
"""
import unreal

MH_ROOT = '/Game/MetaHumans'
RETARGET = MH_ROOT + '/Retarget'
ZOMBIE_RIG = '/Game/ThirdParty/Mixamo/Retarget/IKRig_Zombie'
APPEARANCE_PATH = '/Game/Enemies/Appearance'
HUMAN_LOOKS = APPEARANCE_PATH + '/DA_HumanLooks'
SIREN = '/Game/Enemies/Behaviors/DA_Siren'
LOG = None  # set to a file path to also write the log there

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []


def out(msg):
    lines.append(msg)
    unreal.log_warning('[TH MetaHumans] ' + msg)


def assets_in(path, class_name):
    return [d for d in registry.get_assets_by_path(path, recursive=False) if str(d.asset_class_path.asset_name) == class_name]


def main():
    registry.search_all_assets(True)
    # The MetaHumans: each folder with a Body/SKM_*_BodyMesh and a Face/SKM_*_FaceMesh
    people = []
    for folder in sorted(registry.get_sub_paths(MH_ROOT, False)):
        name = folder.split('/')[-1]
        # The meshes keep the character's own name, which can differ from the folder (2026-11-13: Mara1 holds
        # SKM_MHC_Skotukeda_BodyMesh): any Body/SKM_*_BodyMesh and Face/SKM_*_FaceMesh
        def first(sub, suffix):
            found = [d for d in assets_in('%s/%s' % (folder, sub), 'SkeletalMesh') if str(d.asset_name).endswith(suffix)]
            return found[0].get_asset() if found else None
        body = first('Body', '_BodyMesh')
        face = first('Face', '_FaceMesh')
        if not body or not face:
            continue
        outfit = eal.load_asset('%s/Clothing/%s_Outfits' % (folder, name)) if eal.does_asset_exist('%s/Clothing/%s_Outfits' % (folder, name)) else None
        grooms = []
        for d in assets_in(folder + '/Grooms', 'GroomAsset'):
            groom = d.get_asset()
            binding_path = '%s/Grooms/%s_Binding' % (folder, d.asset_name)
            binding = eal.load_asset(binding_path) if eal.does_asset_exist(binding_path) else None
            grooms.append((groom, binding))
        people.append((name, body, face, outfit, grooms))
        out('%s: body %s, face %s, outfit %s, grooms %s' % (name, body.get_name(), face.get_name(), outfit.get_name() if outfit else None,
                                                            ', '.join('%s%s' % (g.get_name(), '' if b else ' (no binding)') for g, b in grooms)))
    if not people:
        out('no assembled MetaHumans in ' + MH_ROOT)
        return

    # 1. IK rig and retargeter (zombie skeleton -> MetaHuman body skeleton; every MetaHuman body shares it)
    rig_path = RETARGET + '/IKRig_MetaHuman'
    rig = eal.load_asset(rig_path) if eal.does_asset_exist(rig_path) else tools.create_asset('IKRig_MetaHuman', RETARGET, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    rc = unreal.IKRigController.get_controller(rig)
    rc.set_skeletal_mesh(people[0][1])
    rc.apply_auto_generated_retarget_definition()
    eal.save_loaded_asset(rig, False)
    out('IKRig_MetaHuman: %d chains' % len(rc.get_retarget_chains()))

    zombie_rig = eal.load_asset(ZOMBIE_RIG)
    rtg_path = RETARGET + '/RTG_Zombie_To_MetaHuman'
    retargeter = eal.load_asset(rtg_path) if eal.does_asset_exist(rtg_path) else tools.create_asset('RTG_Zombie_To_MetaHuman', RETARGET, unreal.IKRetargeter, unreal.IKRetargetFactory())
    tc = unreal.IKRetargeterController.get_controller(retargeter)
    tc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, zombie_rig)
    tc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rig)
    tc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, people[0][1])
    # UE 5.7: a stack of ops, each with its own rigs and chain map (import_mixamo.py)
    tc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, zombie_rig)
    tc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, rig)
    tc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # The zombie has no palm (metacarpal) chains; fuzzy matching gave them the whole finger, which bent the palms
    for c in rc.get_retarget_chains():
        chain = str(c.get_editor_property('chain_name'))
        if 'Metacarpal' in chain:
            tc.set_source_chain('None', chain, 'FK Chains')
    out('chains: ' + ', '.join('%s<-%s' % (c.get_editor_property('chain_name'), tc.get_source_chain(c.get_editor_property('chain_name'), 'FK Chains'))
                               for c in rc.get_retarget_chains()))
    tc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    eal.save_loaded_asset(retargeter, False)

    # 2. The looks
    humans = eal.load_asset(HUMAN_LOOKS)
    first = humans.get_editor_property('looks')[0]
    stand_in = first.get_editor_property('bodies')[0]
    stand_in_physics = first.get_editor_property('physics')
    out('hidden animated body: %s, physics %s' % (stand_in.get_name(), stand_in_physics.get_name() if stand_in_physics else None))
    looks_path = APPEARANCE_PATH + '/DA_MetaHumanLooks'
    if eal.does_asset_exist(looks_path):
        looks_asset = eal.load_asset(looks_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.THEnemyAppearance)
        looks_asset = tools.create_asset('DA_MetaHumanLooks', APPEARANCE_PATH, unreal.THEnemyAppearance, factory)
    looks = []
    for name, body, face, outfit, grooms in people:
        look = unreal.THEnemyLook()
        look.set_editor_property('name', name)
        look.set_editor_property('weight', 1.0)
        look.set_editor_property('bodies', [body])
        look.set_editor_property('physics', stand_in_physics)
        parts = []
        for part_name, mesh in (('Face', face), ('Outfit', outfit)):
            if mesh:
                choice = unreal.THOutfitChoice()
                choice.set_editor_property('meshes', [mesh])
                part = unreal.THOutfitPart()
                part.set_editor_property('name', part_name)
                part.set_editor_property('choices', [choice])
                parts.append(part)
        look.set_editor_property('parts', parts)
        groom_list = []
        for groom, binding in grooms:
            g = unreal.THGroom()
            g.set_editor_property('groom', groom)
            g.set_editor_property('binding', binding)
            groom_list.append(g)
        look.set_editor_property('grooms', groom_list)
        look.set_editor_property('retargeter', retargeter)
        look.set_editor_property('animated_body', stand_in)
        looks.append(look)
    looks_asset.set_editor_property('looks', looks)
    eal.save_loaded_asset(looks_asset, False)
    out('DA_MetaHumanLooks: %d looks' % len(looks))

    # 3. The siren wears them
    siren = eal.load_asset(SIREN)
    siren.set_editor_property('appearance', looks_asset)
    eal.save_loaded_asset(siren, False)
    out('DA_Siren appearance: DA_MetaHumanLooks')


try:
    main()
except Exception as e:
    out('ERROR %r' % e)
if LOG:
    with open(LOG, 'w') as f:
        f.write('\n'.join(lines))

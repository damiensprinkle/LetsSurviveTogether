"""
Mixamo animations (Adobe, free) for the siren (developer, 2026-11-12: a person's animations only): getting up, lying,
injured idles, punches. Downloaded as FBX, 30 fps, no keyframe reduction, the default Y Bot; one file with skin.

1. Imports the FBX files from SOURCE: the one with skin (SKINNED) brings Mixamo's skeleton and mesh, the rest go onto
   that skeleton. Into /Game/ThirdParty/Mixamo/Source.
2. Makes IK rigs for the Mixamo mesh and the YI zombie mesh (auto retarget chains), and a retargeter between them
   (chains mapped automatically, poses aligned), in /Game/ThirdParty/Mixamo/Retarget.
3. Retargets every clip onto the zombie's UE4 mannequin skeleton: /Game/ThirdParty/Mixamo/Zombie (prefix ZM_).

Re-running re-retargets (replacing); REIMPORT = True imports the FBX files again too. The batch retarget needs the editor running (offscreen), and the
clips only get their skeleton with the older FBX importer (the newer one, Interchange, ignores FbxImportUI), so:
  UnrealEditor.exe "<abs>\LetsSurviveTogether.uproject" -ExecutePythonScript="<abs>/Scripts/Tools/import_mixamo.py" -RenderOffScreen -unattended -nosound "-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=False"
"""
import os
import unreal

SOURCE = r'C:\Users\Damien Sprinkle\Documents\Unreal Projects\Mixamo Animations'
SKINNED = 'Getting Up.fbx'
ROOT = '/Game/ThirdParty/Mixamo'
REIMPORT = False   # True imports the FBX files again over the existing ones
TARGET_MESH ='/Game/ThirdParty/YI_ModularZombies/Meshes/ZombieM04/Zombie/SK_Zombie_M04_01'

tools = unreal.AssetToolsHelpers.get_asset_tools()
imported_mesh = False   # set once the skinned file's mesh is in: its animation gets its own name
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning('[TH Mixamo] ' + msg)


def asset_name(file_name):
    return 'MX_' + os.path.splitext(file_name)[0].replace(' ', '_').replace('(', '').replace(')', '') + ('_Anim' if file_name == SKINNED and imported_mesh else '')


def import_fbx(file_name, skeleton):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, file_name)
    task.destination_path = ROOT + '/Source'
    task.destination_name = asset_name(file_name)
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_animations = True
    options.import_materials = skeleton is None
    options.import_textures = skeleton is None
    if skeleton is None:
        options.import_mesh = True
        options.import_as_skeletal = True
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    else:
        options.import_mesh = False
        options.skeleton = skeleton
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
    options.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task.options = options
    tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.imported_object_paths]


try:
    # The legacy FBX importer (it honours FbxImportUI; Interchange ignores it)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')

    # 1. Import (skipped when already imported, unless REIMPORT)
    existing = None if REIMPORT else unreal.load_asset(ROOT + '/Source/' + asset_name(SKINNED))
    existing = existing if isinstance(existing, unreal.SkeletalMesh) else None
    imported = [existing] if existing else import_fbx(SKINNED, None)
    mesh = next((a for a in imported if isinstance(a, unreal.SkeletalMesh)), None)
    imported_mesh = True
    if not mesh:
        raise RuntimeError('No skeletal mesh from %s (got %s)' % (SKINNED, [a.get_name() for a in imported if a]))
    skeleton = mesh.get_editor_property('skeleton')
    out('mesh %s, skeleton %s' % (mesh.get_path_name(), skeleton.get_path_name()))
    clips = [a for a in imported if isinstance(a, unreal.AnimSequence)]
    for file_name in sorted(os.listdir(SOURCE)):
        # The skinned file too, as an animation (importing it with its mesh doesn't keep its motion). Files already in
        # are skipped unless REIMPORT (developer, 2026-11-14: new clips added to the folder come in on a re-run)
        if not file_name.lower().endswith('.fbx'):
            continue
        if existing and eal.does_asset_exist(ROOT + '/Source/' + asset_name(file_name)):
            continue
        out('importing ' + file_name)
        clips += [a for a in import_fbx(file_name, skeleton) if isinstance(a, unreal.AnimSequence)]
    # Every clip in the folder (the skinned file's own comes in as MX_..._Anim), and everything saved: the importer only
    # saves what it reports, not the skeleton it makes on the side
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    clips = [unreal.load_asset(str(d.package_name)) for d in registry.get_assets_by_path(ROOT + '/Source', recursive=True)
             if str(d.asset_class_path.asset_name) == 'AnimSequence']
    eal.save_directory(ROOT + '/Source', False, True)
    out('%d clips: %s' % (len(clips), ', '.join(c.get_name() for c in clips)))

    # 2. IK rigs and the retargeter
    def ik_rig(name, skeletal_mesh):
        path = ROOT + '/Retarget/' + name
        rig = unreal.load_asset(path) or tools.create_asset(name, ROOT + '/Retarget', unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        controller = unreal.IKRigController.get_controller(rig)
        controller.set_skeletal_mesh(skeletal_mesh)
        controller.apply_auto_generated_retarget_definition()
        eal.save_loaded_asset(rig, False)
        out('%s: %d chains' % (name, len(controller.get_retarget_chains())))
        return rig
    source_rig = ik_rig('IKRig_Mixamo', mesh)
    target = unreal.load_asset(TARGET_MESH)
    target_rig = ik_rig('IKRig_Zombie', target)

    path = ROOT + '/Retarget/RTG_Mixamo_To_Zombie'
    retargeter = unreal.load_asset(path) or tools.create_asset('RTG_Mixamo_To_Zombie', ROOT + '/Retarget', unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(retargeter)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    rc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, mesh)
    rc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target)
    # UE 5.7's retargeter is a stack of ops (pelvis, FK chains, IK goals...), each with its own rigs and chain map:
    # without these, nothing is mapped and the clips come out frozen in the reference pose
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # Mixamo has no root chain; fuzzy matching pairs the zombie's Root with Neck
    rc.set_source_chain('None', 'Root', 'FK Chains')
    out('chains: ' + ', '.join('%s<-%s' % (c.get_editor_property('chain_name'), rc.get_source_chain(c.get_editor_property('chain_name'), 'FK Chains'))
                                 for c in unreal.IKRigController.get_controller(target_rig).get_retarget_chains()))
    # Mixamo stands in a T pose, the YI zombie in an A pose: line the target's bones up with the source's
    rc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    eal.save_loaded_asset(retargeter, False)
    out('retargeter saved')

    # 3. Retarget onto the zombie skeleton
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    # Only clips not retargeted yet (or all with REIMPORT): the animation sets use the existing ZM_ clips, and deleting
    # them would break those references
    if REIMPORT:
        for old in registry.get_assets_by_path(ROOT + '/Zombie', recursive=True):
            eal.delete_asset(str(old.package_name))
    clips = [c for c in clips if REIMPORT or not eal.does_asset_exist(ROOT + '/Zombie/' + 'ZM_' + c.get_name()[3:])]
    out('to retarget: ' + ', '.join(c.get_name() for c in clips))
    datas = [registry.get_asset_by_object_path(c.get_path_name()) for c in clips]
    results = unreal.IKRetargetBatchOperation.duplicate_and_retarget(datas, mesh, target, retargeter, 'MX_', 'ZM_', '', '', False)
    for data in results:
        moved = eal.rename_asset(str(data.package_name), ROOT + '/Zombie/' + str(data.asset_name))
        out('retargeted %s%s' % (data.asset_name, '' if moved else ' (left where it was made)'))
    eal.save_directory(ROOT, False, True)
    out('done')
except Exception as error:
    out('FAILED: %s' % error)

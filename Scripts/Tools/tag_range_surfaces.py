"""
Tags /Game/Maps/L_FiringRange for bullet holes (and footsteps): sets the Physical Material on the materials the range
uses, by what they look like, and Phys Material Override = PM_Paper on the paper targets (their material is shared with
signs and vents, so they are set per actor). Materials not listed stay Default (concrete holes); the script logs them.
Changing a material changes every mesh using it, in any level. Run setup_impact_decals.py first. Safe to re-run;
editor closed.
"""
import unreal

MAP = '/Game/Maps/L_FiringRange'
TEMPLATE_PM = '/Game/Template/LevelPrototyping/PhysicMAT/'
OUR_PM = '/Game/Effects/PhysicalMaterials/'
CONCRETE, WOOD, METAL, PAPER = TEMPLATE_PM + 'PM_Concrete', TEMPLATE_PM + 'PM_Wood', OUR_PM + 'PM_Metal', OUR_PM + 'PM_Paper'
# Material name: physical material. The range kit's ModularParts materials are atlases of its fittings (booths, counters,
# rails, baffles, signs, vents, ceiling panels), all painted or bare metal.
MATERIALS = {
    'FiringRange_ConcreteWalls1': CONCRETE,
    'FiringRange_Floor011': CONCRETE,
    'FiringRange_Floor021': CONCRETE,
    'WarningLine1': CONCRETE,
    'FiringRange_ModularParts01': METAL,
    'FiringRange_ModularParts02': METAL,
    'FiringRange_ModularParts03': METAL,
    'FiringRange_ModularParts04': METAL,
    'FiringRange_ModularParts05': METAL,
    'FiringRange_ModularParts06': METAL,
    'FiringRange_ModularParts07': METAL,
    'FiringRange_WeaponWall1': METAL,
    'FireExtinguisher1': METAL,
    'Light': METAL,
    'MI_OverheadLight_Case': METAL,
    'MI_Valve': METAL,
    'M_Frame': METAL,
    'Crate1': WOOD,
    'WoodenTable1': WOOD,
    'M_Door': WOOD,
}
PAPER_MESHES = ('SM_Target',)

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
loaded = {}
done, untagged, papers = {}, set(), []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.static_mesh and component.static_mesh.get_name() in PAPER_MESHES:
            component.set_phys_material_override(unreal.load_asset(PAPER))
            papers.append(actor.get_actor_label())
        for material in component.get_materials():
            if not material or material.get_path_name() in done:
                continue
            physical = MATERIALS.get(material.get_name())
            if not physical:
                untagged.add(material.get_name())
                continue
            material.set_editor_property('phys_material', loaded.setdefault(physical, unreal.load_asset(physical)))
            if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
                raise RuntimeError('Could not save ' + material.get_path_name())
            done[material.get_path_name()] = physical.split('/')[-1]
if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError('Could not save ' + MAP)
unreal.log('tag_range_surfaces: %d paper targets; materials: %s; left Default: %s' % (len(papers),
    ', '.join('%s=%s' % (k.split('.')[-1], v) for k, v in sorted(done.items())), ', '.join(sorted(untagged))))

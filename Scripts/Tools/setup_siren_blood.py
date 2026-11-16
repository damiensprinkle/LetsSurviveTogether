"""
Blood on the sirens' faces (developer, 2026-11-16: to fit the environment), from the MetaHuman Blood Overlay pack (Lite,
/Game/MHBOP_Lite): its MF_BloodOverlay takes a material's attributes and lays a blood mask over them (colour, normal,
roughness), in the MetaHuman head's UVs. Two masks in the Lite pack: the forehead and over the nose.

1. M_skin_unified_baked_Blood (/Game/Enemies/Materials): a copy of the MetaHumans' baked skin material
   (/Game/MetaHumans/Common/Lookdev_UHM/Skin/Materials/M_skin_unified_baked) with MF_BloodOverlay between its attributes
   and its output. The MetaHumans' own material is left alone.
2. Per siren look in DA_MetaHumanLooks, per head skin slot of its face (head_LOD3, head_LOD57): a material instance of
   that copy (/Game/Enemies/Materials/SirenBlood) with every parameter of the face's own (texture, scalar, vector,
   static switch) and one of the blood masks, the looks taking turns.
3. Each look's Face part wears them (THOutfitChoice → Materials, by slot): the MetaHuman assets aren't changed, so
   re-assembling one in MetaHuman Creator keeps working; re-run this after setup_metahumans.py, which rebuilds the looks.

Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_siren_blood.py" -nullrhi -unattended
"""
import unreal

SKIN = '/Game/MetaHumans/Common/Lookdev_UHM/Skin/Materials/M_skin_unified_baked'
OVERLAY = '/Game/MHBOP_Lite/Materials/MF_BloodOverlay'
FOLDER = '/Game/Enemies/Materials'
BLOOD_SKIN = FOLDER + '/M_skin_unified_baked_Blood'
INSTANCES = FOLDER + '/SirenBlood'
LOOKS = '/Game/Enemies/Appearance/DA_MetaHumanLooks'
HEAD_SLOTS = ['head_LOD3_shader_shader', 'head_LOD57_shader_shader']
MASKS = '/Game/MHBOP_Lite/Materials/textures/BloodMasks/Light/'
# The pack's two masks (its optimized textures), the looks taking turns
BLOOD = [
    {'BloodAlpha': MASKS + 'Forehead/Frontal/T_L_FH_F_Splatter_Alpha_Optimized',
     'BloodNormal': MASKS + 'Forehead/Frontal/T_L_FH_F_Splatter_Normal_Optimized',
     'BloodORM': MASKS + 'Forehead/Frontal/T_L_FH_F_Splatter_OcclusionRoughnessMetallic_Optimized'},
    {'BloodAlpha': MASKS + 'OverNose/Variation/T_L_ON_Var_Splatter_Alpha_Optimized',
     'BloodNormal': MASKS + 'OverNose/Variation/T_L_ON_Var_Splatter_Normal_Optimized',
     'BloodORM': MASKS + 'OverNose/Variation/T_L_ON_Var_Splatter_OcclusionRoughnessMetallic_Optimized'},
]
# The pack demo's values
BLOOD_SCALARS = {'BloodOverlayOpacity': 1.0, 'BloodThickness': 1.0, 'BloodMetallicStrenght': 1.0}
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ATTRIBUTES = unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES


def out(msg):
    unreal.log_warning('[TH siren blood] ' + str(msg))


def load(path):
    asset = eal.load_asset(path)
    if not asset:
        raise RuntimeError('missing ' + path)
    return asset


def blood_skin():
    """The skin material with the overlay on its output (made once; a re-run keeps it)."""
    overlay = load(OVERLAY)
    if eal.does_asset_exist(BLOOD_SKIN):
        material = load(BLOOD_SKIN)
        node = mel.get_material_property_input_node(material, ATTRIBUTES)
        if isinstance(node, unreal.MaterialExpressionMaterialFunctionCall) and node.get_editor_property('material_function') == overlay:
            out('%s: already has the overlay' % BLOOD_SKIN.split('/')[-1])
            return material
        raise RuntimeError('%s exists without the overlay on its output: delete it and re-run' % BLOOD_SKIN)
    material = eal.duplicate_asset(SKIN, BLOOD_SKIN)
    if not material:
        raise RuntimeError('could not copy ' + SKIN)
    before = mel.get_material_property_input_node(material, ATTRIBUTES)
    before_out = mel.get_material_property_input_node_output_name(material, ATTRIBUTES)
    if not before:
        raise RuntimeError('the skin material has nothing on its attributes output')
    call = mel.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall,
                                          before.get_editor_property('material_expression_editor_x') + 300,
                                          before.get_editor_property('material_expression_editor_y'))
    call.set_editor_property('material_function', overlay)
    inputs = [str(n) for n in mel.get_material_expression_input_names(call)]
    out('overlay inputs: %s' % inputs)
    if not inputs:
        raise RuntimeError('the overlay call has no inputs (its function did not take)')
    into = next((n for n in inputs if 'attribute' in n.lower()), inputs[0])
    if not mel.connect_material_expressions(before, before_out, call, into):
        raise RuntimeError('could not connect the skin attributes to the overlay')
    if not mel.connect_material_property(call, '', ATTRIBUTES):
        raise RuntimeError('could not connect the overlay to the output')
    mel.recompile_material(material)
    eal.save_loaded_asset(material, False)
    out('%s: %s.%s -> MF_BloodOverlay(%s) -> attributes' % (BLOOD_SKIN.split('/')[-1], type(before).__name__, before_out, into))
    return material


def copy_parameters(source, target):
    """Every parameter of source (its own and inherited, resolved) onto target."""
    counts = [0, 0, 0, 0]
    for name in mel.get_scalar_parameter_names(source):
        mel.set_material_instance_scalar_parameter_value(target, name, mel.get_material_instance_scalar_parameter_value(source, name))
        counts[0] += 1
    for name in mel.get_vector_parameter_names(source):
        mel.set_material_instance_vector_parameter_value(target, name, mel.get_material_instance_vector_parameter_value(source, name))
        counts[1] += 1
    for name in mel.get_texture_parameter_names(source):
        value = mel.get_material_instance_texture_parameter_value(source, name)
        if value:
            mel.set_material_instance_texture_parameter_value(target, name, value)
        counts[2] += 1
    for name in mel.get_static_switch_parameter_names(source):
        mel.set_material_instance_static_switch_parameter_value(target, name, mel.get_material_instance_static_switch_parameter_value(source, name))
        counts[3] += 1
    return counts


def main():
    material = blood_skin()
    looks_asset = load(LOOKS)
    looks = []
    for index, look in enumerate(looks_asset.get_editor_property('looks')):
        looks.append(look)
        name = str(look.get_editor_property('name'))
        blood = BLOOD[index % len(BLOOD)]
        parts = []
        for part in look.get_editor_property('parts'):
            parts.append(part)
            if str(part.get_editor_property('name')) != 'Face':
                continue
            choices = []
            for choice in part.get_editor_property('choices'):
                choices.append(choice)
                meshes = [m for m in choice.get_editor_property('meshes') if m]
                if not meshes:
                    continue
                slots = {str(s.get_editor_property('material_slot_name')): s.get_editor_property('material_interface') for s in meshes[0].get_editor_property('materials')}
                picks = []
                for slot in HEAD_SLOTS:
                    original = slots.get(slot)
                    if not original:
                        out('%s: no %s' % (name, slot))
                        continue
                    asset_name = 'MI_%s_%s_Blood' % (name, slot.replace('_shader_shader', ''))
                    path = INSTANCES + '/' + asset_name
                    if eal.does_asset_exist(path):
                        eal.delete_asset(path)
                    instance = tools.create_asset(asset_name, INSTANCES, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                    mel.set_material_instance_parent(instance, material)
                    counts = copy_parameters(original, instance)
                    for key, texture in blood.items():
                        mel.set_material_instance_texture_parameter_value(instance, key, load(texture))
                    for key, value in BLOOD_SCALARS.items():
                        mel.set_material_instance_scalar_parameter_value(instance, key, value)
                    mel.update_material_instance(instance)
                    eal.save_loaded_asset(instance, False)
                    pick = unreal.THMaterialChoice()
                    pick.set_editor_property('slot', slot)
                    pick.set_editor_property('materials', [instance])
                    picks.append(pick)
                    out('%s %s: %s (from %s: %d scalars, %d vectors, %d textures, %d switches), blood %s' % (
                        name, slot, asset_name, original.get_name(), counts[0], counts[1], counts[2], counts[3],
                        blood['BloodAlpha'].split('/')[-1]))
                choice.set_editor_property('materials', picks)
            part.set_editor_property('choices', choices)
        look.set_editor_property('parts', parts)
    looks_asset.set_editor_property('looks', looks)
    eal.save_loaded_asset(looks_asset, False)
    out('DA_MetaHumanLooks saved')


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

"""
Blood on the screen when the local player is hit (UTHStatusWidget::ShowHit).

Makes /Game/UI/M_UI_ScreenBlood: a User Interface material, translucent, drawing one splatter:
  colour  = vector parameter Tint
  opacity = texture parameter Mask (its red channel: Yarrawah Interactive's T_Blood_Splatter_*_Single_Mask) x scalar Opacity
The widget picks a mask per splatter and fades Opacity. Re-running rebuilds the material in place.

Headless:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/setup_hit_feedback.py" -nullrhi -unattended
"""
import unreal

PATH = '/Game/UI'
NAME = 'M_UI_ScreenBlood'
MASK = '/Game/ThirdParty/YI_BloodDecals/Textures/Singles/T_Blood_Splatter_01_Single_Mask'

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def out(text):
    unreal.log_warning('[TH HitFeedback] ' + text)


def sampler_for(texture):
    """The sampler type a texture parameter needs for this texture's compression."""
    compression = texture.get_editor_property('compression_settings')
    srgb = texture.get_editor_property('srgb')
    if compression == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if compression in (unreal.TextureCompressionSettings.TC_GRAYSCALE, unreal.TextureCompressionSettings.TC_ALPHA):
        return unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


try:
    mask = unreal.load_asset(MASK)
    if not mask:
        raise RuntimeError('Missing ' + MASK)
    full = PATH + '/' + NAME
    material = unreal.load_asset(full)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(material)
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    sample = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
    sample.set_editor_property('parameter_name', 'Mask')
    sample.set_editor_property('texture', mask)
    sample.set_editor_property('sampler_type', sampler_for(mask))

    opacity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 250)
    opacity.set_editor_property('parameter_name', 'Opacity')
    opacity.set_editor_property('default_value', 1.0)

    tint = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.32, 0.0, 0.0, 1.0))

    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(sample, 'R', multiply, 'A')
    mel.connect_material_expressions(opacity, '', multiply, 'B')

    mel.connect_material_property(tint, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(multiply, '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    if not eal.save_asset(full, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + full)
    out('%s made (mask %s, compression %s)' % (full, MASK.split('/')[-1], mask.get_editor_property('compression_settings')))
except Exception as error:
    out('FAILED: %s' % error)

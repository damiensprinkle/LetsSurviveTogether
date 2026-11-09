"""
Bullet holes by surface from the Fab pack "Bullet Impact Decals - Combo Pack" (Content/ThirdParty/Bullet_Holes).

Creates (or updates) /Game/Effects/DA_ImpactSurfaces (UTHImpactSurfaces) and points every gun's data asset at it.
Surface types are in Config/DefaultEngine.ini (Project Settings, Physics, Physical Surface):
  Default (no Physical Material): Concrete     Concrete (1): Concrete     Dirt (2): Default
  Wood (3): Wood     Metal (4): Metal     Plaster (5): Plaster     Paper (6): PAPER_SET (range targets)
Each set is the pack's 10 variants (MI_Parallax_Decal_<Set>_0..9); a shot picks one at random.
Also creates the Physical Materials for the surfaces the template has none for: /Game/Effects/PhysicalMaterials/
PM_Metal, PM_Plaster, PM_Paper (PM_Concrete, PM_Wood, PM_Dirt are in /Game/Template/LevelPrototyping/PhysicMAT).
To tag something: set a Physical Material on its material, or Phys Material Override on a placed actor (TECHNICAL_ARCHITECTURE.md).
Safe to re-run. Run headless with the editor closed (see README.md).
"""
import unreal

PACK = '/Game/ThirdParty/Bullet_Holes/Materials'
ASSET = '/Game/Effects/DA_ImpactSurfaces'
PHYSICAL = '/Game/Effects/PhysicalMaterials'
DEFAULT_SET = 'Concrete'
PAPER_SET = 'Plaster'
S = unreal.PhysicalSurface
SURFACES = {
    S.SURFACE_TYPE1: 'Concrete',
    S.SURFACE_TYPE3: 'Wood',
    S.SURFACE_TYPE4: 'Metal',
    S.SURFACE_TYPE5: 'Plaster',
    S.SURFACE_TYPE6: PAPER_SET,
}
NEW_PHYSICAL = {'PM_Metal': S.SURFACE_TYPE4, 'PM_Plaster': S.SURFACE_TYPE5, 'PM_Paper': S.SURFACE_TYPE6}
GUNS = ['/Game/Weapons/Data/DA_M4', '/Game/Weapons/Data/DA_G17']
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def decal_set(name):
    impact = unreal.THSurfaceImpact()
    decals = []
    for i in range(10):
        path = '%s/%s/MI_Parallax_Decal_%s_%d' % (PACK, name, name, i)
        material = unreal.load_asset(path)
        if not material:
            raise RuntimeError('Missing decal ' + path)
        decals.append(material)
    impact.set_editor_property('decals', decals)
    return impact


for name, surface in NEW_PHYSICAL.items():
    path = PHYSICAL + '/' + name
    physical = unreal.load_asset(path) or tools.create_asset(name, PHYSICAL, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
    physical.set_editor_property('surface_type', surface)
    if not eal.save_loaded_asset(physical, False):
        raise RuntimeError('Could not save ' + path + ' (is the editor open?)')

data = unreal.load_asset(ASSET)
if not data:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.THImpactSurfaces)
    data = tools.create_asset('DA_ImpactSurfaces', '/Game/Effects', unreal.THImpactSurfaces, factory)
data.set_editor_property('default', decal_set(DEFAULT_SET))
data.set_editor_property('surfaces', {surface: decal_set(name) for surface, name in SURFACES.items()})
if not eal.save_loaded_asset(data, False):
    raise RuntimeError('Could not save ' + ASSET + ' (is the editor open?)')

for path in GUNS:
    gun = unreal.load_asset(path)
    gun.set_editor_property('impact_surfaces', data)
    if not eal.save_loaded_asset(gun, False):
        raise RuntimeError('Could not save ' + path + ' (is the editor open?)')

unreal.log('setup_impact_decals: %s with Default %s, %d surfaces (paper: %s), on %d guns' % (ASSET, DEFAULT_SET, len(SURFACES), PAPER_SET, len(GUNS)))

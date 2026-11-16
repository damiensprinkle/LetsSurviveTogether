"""
Bullet holes by surface from the Fab pack "Bullet Impact Decals - Combo Pack" (Content/ThirdParty/Bullet_Holes).

Creates (or updates) /Game/Effects/DA_ImpactSurfaces (UTHImpactSurfaces) and points every gun's data asset at it.
Surface types are in Config/DefaultEngine.ini (Project Settings, Physics, Physical Surface):
  Default (no Physical Material): Concrete     Concrete (1): Concrete     Dirt (2): Default
  Wood (3): Wood     Metal (4): Metal     Plaster (5): Plaster     Paper (6): PAPER_SET (range targets)
  Flesh (7): no hole; blood from the Yarrawah Interactive packs (YI_BloodDroplets wound effects, YI_BloodDecals
             splatter on the wall or floor behind the hit); sounds from High Impact Media's Bullet SFX - Flesh:
             Flesh on body shots, Guts on headshots, Character_Hit when the body is a player's (friendly fire),
             all through /Game/Audio/SA_BulletImpact (made here) so they fade with distance
Each set is the pack's 10 variants (MI_Parallax_Decal_<Set>_0..9); a shot picks one at random.
Also creates the Physical Materials for the surfaces the template has none for: /Game/Effects/PhysicalMaterials/
PM_Metal, PM_Plaster, PM_Paper, PM_Flesh (PM_Concrete, PM_Wood, PM_Dirt are in /Game/Template/LevelPrototyping/PhysicMAT).
PM_Flesh goes on bodies' bones from C++ (ATHEnemyCharacter::FleshMaterial, UTHWeaponComponent::BodyPhysicalMaterial).
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
FLESH = S.SURFACE_TYPE7
NEW_PHYSICAL = {'PM_Metal': S.SURFACE_TYPE4, 'PM_Plaster': S.SURFACE_TYPE5, 'PM_Paper': S.SURFACE_TYPE6, 'PM_Flesh': FLESH}
BLOOD_FX = '/Game/ThirdParty/YI_BloodDroplets/Particles/CPU/'
BLOOD_ENTRANCE = BLOOD_FX + 'FX_EntranceWound_Rifle_CPU'
BLOOD_EXIT = BLOOD_FX + 'FX_ExitWound_Rifle_CPU'
BLOOD_SPLATTER = ['/Game/ThirdParty/YI_BloodDecals/Materials/Instances/Singles/MI_BloodSplatter_Small_%02d' % i for i in range(1, 7)]
# High Impact Media's Bullet SFX - Flesh (ThirdParty/BulletSFX_Flesh): 20 cues per kind; "NoLeadIn" starts on the hit
HIT_SFX = '/Game/ThirdParty/BulletSFX_Flesh/%s/CUE/No_Lead_in/Bullet_SFX_NoLeadIn_%s_%02d_Cue'
FLESH_SOUNDS = [HIT_SFX % ('Flesh', 'Flesh', i) for i in range(1, 21)]                    # body shots on enemies
HEADSHOT_SOUNDS = [HIT_SFX % ('Guts', 'Guts', i) for i in range(1, 21)]                   # headshots
PLAYER_HIT_SOUNDS = [HIT_SFX % ('Character_Hit', 'Character_Hit', i) for i in range(1, 21)]  # a player's body (friendly fire)
# Impact sounds fade with distance (the pack's cues have no attenuation of their own)
ATTENUATION = '/Game/Audio/SA_BulletImpact'
ATTENUATION_INNER = 300.0     # full volume within 3 m
ATTENUATION_FALLOFF = 3500.0  # silent 35 m beyond that
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


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('Missing ' + path)
    return asset


# The YI wound effects' own controls (1 = as authored): the spray kept going too long, so shorter spawning and droplets
# (2026-11-10, developer: still a bit much, so fewer droplets that live shorter and fall faster).
# Tune in DA_ImpactSurfaces → Surfaces → Flesh → Effect Parameters, or here and re-run.
BLOOD_PARAMETERS = {
    'EmitterDurationCoefficient': 0.25,
    'ParticleLifeTimeCoefficient': 0.35,
    'ParticleAmountCoefficient': 0.6,
    'GravityCoefficient': 1.5,
}
# The droplets' collision channel. Their own (WorldStatic) is blocked by living bodies' invisible capsules, so droplets
# landed on them and hung in the air; BloodDroplets (DefaultEngine.ini) is ignored by capsules.
BLOOD_DROPLET_CHANNEL = 'ECC_BLOOD_DROPLETS'    # unreal.CollisionChannel: ECC_GameTraceChannel2, named in DefaultEngine.ini


def flesh():
    """No bullet hole: wound effects at the hit, blood on whatever is behind."""
    impact = unreal.THSurfaceImpact()
    impact.set_editor_property('effect', load(BLOOD_ENTRANCE))
    impact.set_editor_property('exit_effect', load(BLOOD_EXIT))
    impact.set_editor_property('effect_parameters', BLOOD_PARAMETERS)
    impact.set_editor_property('set_effect_collision_channel', True)
    channel = getattr(unreal.CollisionChannel, BLOOD_DROPLET_CHANNEL, None)
    if channel is None:
        raise RuntimeError('No CollisionChannel.%s; there are: %s' % (BLOOD_DROPLET_CHANNEL, [n for n in dir(unreal.CollisionChannel) if n.isupper()]))
    impact.set_editor_property('effect_collision_channel', channel)
    impact.set_editor_property('splatter_decals', [load(p) for p in BLOOD_SPLATTER])
    impact.set_editor_property('splatter_distance', 250.0)
    impact.set_editor_property('splatter_size', 60.0)
    impact.set_editor_property('sounds', [load(p) for p in FLESH_SOUNDS])
    impact.set_editor_property('headshot_sounds', [load(p) for p in HEADSHOT_SOUNDS])
    return impact


def attenuation():
    asset = unreal.load_asset(ATTENUATION)
    if not asset:
        name = ATTENUATION.split('/')[-1]
        asset = tools.create_asset(name, ATTENUATION.rsplit('/', 1)[0], unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = asset.get_editor_property('attenuation')
    settings.set_editor_property('attenuate', True)
    settings.set_editor_property('spatialize', True)
    settings.set_editor_property('attenuation_shape', unreal.AttenuationShape.SPHERE)
    settings.set_editor_property('attenuation_shape_extents', unreal.Vector(ATTENUATION_INNER, 0, 0))
    settings.set_editor_property('falloff_distance', ATTENUATION_FALLOFF)
    asset.set_editor_property('attenuation', settings)
    if not eal.save_loaded_asset(asset, False):
        raise RuntimeError('Could not save ' + ATTENUATION)
    return asset


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
surfaces = {surface: decal_set(name) for surface, name in SURFACES.items()}
surfaces[FLESH] = flesh()
data.set_editor_property('surfaces', surfaces)
data.set_editor_property('player_hit_sounds', [load(p) for p in PLAYER_HIT_SOUNDS])
data.set_editor_property('sound_attenuation', attenuation())
if not eal.save_loaded_asset(data, False):
    raise RuntimeError('Could not save ' + ASSET + ' (is the editor open?)')

for path in GUNS:
    gun = unreal.load_asset(path)
    gun.set_editor_property('impact_surfaces', data)
    if not eal.save_loaded_asset(gun, False):
        raise RuntimeError('Could not save ' + path + ' (is the editor open?)')

unreal.log('setup_impact_decals: %s with Default %s, %d surfaces (paper: %s) and flesh, on %d guns' % (ASSET, DEFAULT_SET, len(SURFACES), PAPER_SET, len(GUNS)))

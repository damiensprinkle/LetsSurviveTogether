"""
Iron sights for aiming down sights: sets SightBone, RearSight and FrontSight on each gun's data asset. The points are in
the weapon mesh's own space at its reference pose, measured from the meshes with the Tools.ProfileMeshes test
(Saved/MeshProfiles/<mesh>.txt: the gun's top edge along the barrel, which runs along +Y with +Z up):
  M4 (bone Main): rail top Z 149.7; rear sight block at Y 2..7 (top 153.8), front sight post at Y 44..47 (tip 153.4).
     The sight line runs over the post's tip, level with the bore (X -5.65), so both points are at Z 153.4.
  G17 (bone Slide, so the sights ride the slide): slide top Z 150.5; rear sight at Y 8..9 (top 151.1), front sight at
     Y 26 (top 151.0), X -4.85.
Tune in play with the console: th.SightTune "rearUp frontUp distance", then add the numbers here and re-run.
Safe to re-run; editor closed, headless.
"""
import unreal

SIGHTS = {
    '/Game/Weapons/Data/DA_M4': ('Main', unreal.Vector(-5.65, 5.0, 153.4), unreal.Vector(-5.65, 45.5, 153.4), 0.0),
    '/Game/Weapons/Data/DA_G17': ('Slide', unreal.Vector(-4.85, 8.5, 151.0), unreal.Vector(-4.85, 26.0, 151.0), 0.0),
}

for path, (bone, rear, front, distance) in SIGHTS.items():
    data = unreal.load_asset(path)
    data.set_editor_property('sight_bone', bone)
    data.set_editor_property('rear_sight', rear)
    data.set_editor_property('front_sight', front)
    data.set_editor_property('aim_sight_distance', distance)
    if not unreal.EditorAssetLibrary.save_loaded_asset(data, False):
        raise RuntimeError('Could not save ' + path + ' (is the editor open?)')
    unreal.log('setup_iron_sights: %s bone %s rear %s front %s' % (data.get_name(), bone, rear, front))

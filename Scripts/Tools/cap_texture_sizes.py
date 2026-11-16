"""
Caps every texture in /Game at MAX pixels in game (developer, 2026-11-13: texture streaming pool over budget; we don't
need 8K textures, 2K at most). Sets each larger texture's Maximum Texture Size; the full-size source stays in the asset,
so raising MAX later and re-running brings detail back. Reads sizes from the asset registry (no loading) and loads only
the textures it changes.

Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/cap_texture_sizes.py" -nullrhi -unattended
"""
import unreal

MAX = 2048
ROOT = '/Game'
CLASSES = ['Texture2D', 'TextureCube', 'Texture2DArray']
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def out(msg):
    unreal.log_warning('[TH textures] ' + str(msg))


def size_of(data):
    # "Dimensions" tag: "8192x8192" (or "2048x2048x6" for cubes)
    dims = str(data.get_tag_value('Dimensions') or '')
    try:
        return max(int(v) for v in dims.split('x')[:2])
    except ValueError:
        return 0


def main():
    registry.search_all_assets(True)
    changed = skipped = unknown = 0
    by_folder = {}
    for data in registry.get_assets_by_path(ROOT, recursive=True):
        if str(data.asset_class_path.asset_name) not in CLASSES:
            continue
        size = size_of(data)
        if size == 0:
            unknown += 1
            continue
        if size <= MAX:
            continue
        texture = unreal.load_asset(str(data.package_name))
        if not texture:
            continue
        current = texture.get_editor_property('max_texture_size')
        if current and current <= MAX:
            skipped += 1
            continue
        texture.set_editor_property('max_texture_size', MAX)
        if eal.save_loaded_asset(texture, False):
            changed += 1
            folder = '/'.join(str(data.package_path).split('/')[:4])
            by_folder[folder] = by_folder.get(folder, 0) + 1
            if changed % 50 == 0:
                unreal.SystemLibrary.collect_garbage()
                out('... %d' % changed)
    out('capped %d textures at %d (already capped %d, size unknown %d)' % (changed, MAX, skipped, unknown))
    for folder, count in sorted(by_folder.items(), key=lambda kv: -kv[1]):
        out('   %s: %d' % (folder, count))


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

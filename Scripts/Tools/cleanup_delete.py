"""Content cleanup, step 1: delete content the game doesn't use and won't need (2026-11-04, developer's call).

Everything deleted stays in git history: restore a folder with
    git checkout <commit before the cleanup> -- "Content/<folder>"

Dry run by default (reports what refers to the doomed assets from outside the list). Set TH_DELETE=1 to delete.
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()

FOLDERS = [
    "/Game/MetaHumans",                 # template's Koda MetaHuman, in neither map
    "/Game/Variant_Horror",             # UE first-person template's horror variant
    "/Game/__ExternalActors__/Variant_Horror",
    "/Game/__ExternalObjects__/Variant_Horror",
    "/Game/FirstPerson",                # UE first-person template level and blueprints
    "/Game/__ExternalActors__/FirstPerson",
    "/Game/__ExternalObjects__/FirstPerson",
    "/Game/Input",                      # UE first-person template input (the horror template's is in Blueprint/Input)
    "/Game/Fab/M4_-_FPS_Weapon_Animations_Pack_FREE__v_1_",   # Fab copies; we use our imports in Weapons/M4_T0
    "/Game/Fab/G17_-_FPS_Weapon_Animations_Pack__v_1_",       # and Weapons/G17
    "/Game/Weapons/Rifle",              # early placeholder guns
    "/Game/Weapons/Pistol",
    "/Game/Weapons/GrenadeLauncher",
]
ASSETS = [
    "/Game/Weapons/Blueprints/BP_Weapon_Pistol",
    "/Game/Weapons/Blueprints/BP_Weapon_Rifle",
    "/Game/Weapons/Data/DA_Pistol",
    "/Game/Weapons/Data/DA_Rifle",
    # G17 importer leftovers (the gun uses MI_Glock / MI_Glove / MI_Hand)
    "/Game/Weapons/G17/Glock_D", "/Game/Weapons/G17/Glock_D_ncl1_1",
    "/Game/Weapons/G17/Glove_D", "/Game/Weapons/G17/Glove_D_ncl1_1",
    "/Game/Weapons/G17/Hand_D", "/Game/Weapons/G17/Hand_D_ncl1_1",
]


def out(msg):
    unreal.log_warning("[TH Delete] " + msg)


content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())


def packages_in(folder):
    base = os.path.join(content_dir, *folder[len("/Game/"):].split("/"))
    result = []
    for dirpath, _, files in os.walk(base):
        for name in files:
            if name.endswith((".uasset", ".umap")):
                rel = os.path.relpath(os.path.join(dirpath, name), content_dir).replace("\\", "/")
                result.append("/Game/" + rel.rsplit(".", 1)[0])
    return result


doomed = set(ASSETS)
for folder in FOLDERS:
    doomed.update(packages_in(folder))
out("%d packages to delete" % len(doomed))

options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
outside = {}
for package in doomed:
    for ref in registry.get_referencers(package, options) or []:
        ref = str(ref)
        if ref not in doomed:
            outside.setdefault(ref, set()).add(package)
for ref, targets in sorted(outside.items()):
    out("STILL REFERS: %s -> %s" % (ref, ", ".join(sorted(targets))[:300]))
out("%d packages outside the list refer to deleted ones" % len(outside))

if os.environ.get("TH_DELETE") == "1":
    for folder in FOLDERS:
        if eal.does_directory_exist(folder):
            ok = eal.delete_directory(folder)
            out("deleted folder %s: %s" % (folder, ok))
    for asset in ASSETS:
        if eal.does_asset_exist(asset):
            ok = eal.delete_asset(asset)
            out("deleted %s: %s" % (asset, ok))
    # Anything the editor wouldn't delete (e.g. external actor files) goes from disk
    for folder in FOLDERS:
        path = os.path.join(content_dir, *folder[len("/Game/"):].split("/"))
        if os.path.isdir(path):
            for dirpath, dirs, files in os.walk(path, topdown=False):
                for name in files:
                    os.remove(os.path.join(dirpath, name))
                os.rmdir(dirpath)
            out("removed from disk: %s" % folder)
else:
    out("dry run; set TH_DELETE=1 to delete")

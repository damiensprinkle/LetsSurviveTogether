"""Content cleanup, steps 2 and 3 (2026-11-04): archive unused animations, then move packs into ThirdParty/ and the
horror template into Template/. Uses the editor's own rename, so references follow, then fixes up the redirectors.

  TH_STEP=archive   unused animations (from Saved/ContentInventory_Unused.txt, run content_inventory.py first)
                    -> /Game/_Archive/Animations/<pack>/<their path inside the pack>
  TH_STEP=move      the folder moves in MOVES below (headless, folder by folder)
  TH_STEP=fixup     fix every remaining redirector; run in the full editor (-ExecutePythonScript, TH_QUIT=1) so maps
                    that reference moved assets get re-saved
  TH_DRY=1          only print what would happen

After it: build, then update hard-coded paths (C++, Config, Scripts) to the new locations.
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()

STEP = os.environ.get("TH_STEP", "")
DRY = os.environ.get("TH_DRY") == "1"

# Source folder -> name inside _Archive/Animations
ARCHIVE_FROM = {
    "/Game/Blueprint/Lyra_Content": "Lyra",
    "/Game/SampleAnimationPack": "SampleAnimationPack",
    "/Game/Characters/Mannequins": "Mannequins",
    "/Game/FPFlashlightAnims": "FPFlashlightAnims",
    "/Game/Weapons": "Weapons",
}
ANIM_CLASSES = {"AnimSequence", "AnimMontage", "AnimComposite", "BlendSpace", "BlendSpace1D", "AimOffsetBlendSpace",
                "AimOffsetBlendSpace1D", "PoseAsset"}

# Order matters: Lyra leaves Blueprint before Blueprint moves
MOVES = [
    ("/Game/Blueprint/Lyra_Content", "/Game/ThirdParty/Lyra"),
    ("/Game/SampleAnimationPack", "/Game/ThirdParty/SampleAnimationPack"),
    ("/Game/FPFlashlightAnims", "/Game/ThirdParty/FPFlashlightAnims"),
    ("/Game/WeaponsFoleySFX", "/Game/ThirdParty/WeaponsFoleySFX"),
    ("/Game/FreeWeaponSFX", "/Game/ThirdParty/FreeWeaponSFX"),
    ("/Game/Fab/FREE_Tactical_Flashlight_-_Game_Ready", "/Game/ThirdParty/TacticalFlashlight"),
    ("/Game/LevelPrototyping", "/Game/Template/LevelPrototyping"),
    ("/Game/Interaction", "/Game/Template/Interaction"),
    # ("/Game/Blueprint", "/Game/Template/Blueprint"),  crashed the editor twice when scripted; move it by hand if wanted
]


def out(msg):
    unreal.log_warning("[TH Reorg] " + msg)


content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
saved_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())


def packages_under(folder):
    base = os.path.join(content_dir, *folder[len("/Game/"):].split("/"))
    result = []
    for dirpath, _, files in os.walk(base):
        for name in files:
            if name.endswith((".uasset", ".umap")):
                rel = os.path.relpath(os.path.join(dirpath, name), content_dir).replace("\\", "/")
                result.append("/Game/" + rel.rsplit(".", 1)[0])
    return result


def rename(pairs):
    """pairs: (package, new folder). Renames in batches through AssetTools so references are fixed up."""
    data = []
    for package, new_folder in pairs:
        asset = eal.load_asset(package)
        if not asset:
            out("could not load " + package)
            continue
        data.append(unreal.AssetRenameData(asset, new_folder, package.rsplit("/", 1)[1]))
    # One call: the editor loads every referencer before renaming anything. In batches, assets referring to an
    # earlier batch fail to load against its fresh redirectors.
    ok = tools.rename_assets(data)
    out("renamed %d assets: %s" % (len(data), ok))


def fix_redirectors(folders):
    """Only reports: Python has no FixupReferencers. Fix them with the ResavePackages commandlet afterwards:
    UnrealEditor-Cmd <uproject> -run=ResavePackages -fixupredirects -projectonly -unattended"""
    count = 0
    for folder in folders:
        for package in packages_under(folder):
            if any(str(a.asset_class_path.asset_name) == "ObjectRedirector" for a in registry.get_assets_by_package_name(package)):
                count += 1
    out("%d redirectors to fix with ResavePackages -fixupredirects" % count)


def remove_empty_dirs(folder):
    base = os.path.join(content_dir, *folder[len("/Game/"):].split("/"))
    for dirpath, dirs, files in os.walk(base, topdown=False):
        if not os.listdir(dirpath):
            os.rmdir(dirpath)


if STEP == "archive":
    unused = []
    with open(os.path.join(saved_dir, "ContentInventory_Unused.txt"), encoding="utf-8") as f:
        for line in f:
            package, cls = line.split("\t")[:2]
            if cls in ANIM_CLASSES:
                unused.append(package)
    pairs = []
    for package in unused:
        for source, name in ARCHIVE_FROM.items():
            if package.startswith(source + "/"):
                inner = package[len(source) + 1:].rsplit("/", 1)[0] if "/" in package[len(source) + 1:] else ""
                pairs.append((package, "/Game/_Archive/Animations/%s%s" % (name, "/" + inner if inner else "")))
                break
    out("%d unused animations to archive" % len(pairs))
    for package, folder in pairs[:15]:
        out("  %s -> %s" % (package, folder))
    if not DRY:
        rename(pairs)
        fix_redirectors(list(ARCHIVE_FROM.keys()) + ["/Game/_Archive"])
        for source in ARCHIVE_FROM:
            remove_empty_dirs(source)

elif STEP == "move":
    # Folder by folder (one huge rename crashed the editor), each followed by its redirector fix-up. A folder skips
    # anything inside an earlier-listed one (Blueprint/Lyra_Content), so redirectors left by that move aren't moved
    # again (which chains them). Run headless, then TH_STEP=fixup in the full editor for what references maps hold.
    done_sources = []
    for source, target in MOVES:
        packages = [p for p in packages_under(source) if not any(p.startswith(d + "/") for d in done_sources)]
        # Skip redirectors left by an earlier run: loading one returns the moved asset, which would move it again
        packages = [p for p in packages if not any(str(a.asset_class_path.asset_name) == "ObjectRedirector" for a in registry.get_assets_by_package_name(p))]
        done_sources.append(source)
        out("%s -> %s (%d packages)" % (source, target, len(packages)))
        if DRY or not packages:
            continue
        pairs = []
        for package in packages:
            inner = package[len(source) + 1:]
            sub = inner.rsplit("/", 1)[0] if "/" in inner else ""
            pairs.append((package, target + ("/" + sub if sub else "")))
        rename(pairs)

elif STEP == "fixup":
    fix_redirectors(["/Game"])
    for source, _ in MOVES:
        remove_empty_dirs(source)

elif STEP != "archive":
    out("set TH_STEP=archive, move or fixup")

out("finished")
# Run in the full editor (maps can only be re-saved there): -ExecutePythonScript with TH_QUIT=1 closes it afterwards
if os.environ.get("TH_QUIT") == "1":
    unreal.SystemLibrary.quit_editor()

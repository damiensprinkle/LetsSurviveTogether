"""Content inventory: which assets the game uses, per folder. Changes nothing.

Starts from what the game loads (the maps it plays, the character the game mode spawns, assets named in config and C++)
and follows every reference (hard and soft) to find everything in use. Everything else under /Game is reported as unused.
Writes Saved/ContentInventory.md (summary per folder) and Saved/ContentInventory_Unused.txt (every unused asset).

Run headless:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
Keep ROOTS in step with Config/*.ini and the C++ (grep for "/Game/").
"""
import os
import unreal

ROOTS = [
    "/Game/Demo",
    "/Game/Maps/MainMenu",
    "/Game/Blueprint/Character/BP_FirstPersonCharacter",   # THGameMode's default pawn
    "/Game/Weapons/Data/DA_HandFlashlight",                 # ATHHandFlashlight defaults
    "/Game/ThirdParty/FPFlashlightAnims/Demo/Player/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms",
    "/Game/ThirdParty/FPFlashlightAnims/Mesh/SM_Flashlight",
    "/Game/Weapons/HandFlashlight/Animations/A_FP_HandFlashlight_Inspect",
] + ["/Game/UI/Crosshairs/T_CH00%d" % i for i in range(1, 9)]

MAPS = ["Demo", "Maps/MainMenu"]

# World Partition maps keep their actors in separate packages (__ExternalActors__ / __ExternalObjects__) that the map
# package itself doesn't reference, so each of those is a starting point too
content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
for map_path in MAPS:
    for external in ("__ExternalActors__", "__ExternalObjects__"):
        base = os.path.join(content_dir, external, *map_path.split("/"))
        for dirpath, _, files in os.walk(base):
            for name in files:
                if name.endswith(".uasset"):
                    rel = os.path.relpath(os.path.join(dirpath, name), content_dir).replace("\\", "/")[:-len(".uasset")]
                    ROOTS.append("/Game/" + rel)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                include_searchable_names=False, include_soft_management_references=False,
                                                include_hard_management_references=False)

used = set()
stack = [r for r in ROOTS]
while stack:
    package = stack.pop()
    if package in used or not package.startswith("/Game/"):
        continue
    used.add(package)
    if "/__External" in package:
        # Load-free: external actor packages are in the registry by package name only
        pass
    for dep in registry.get_dependencies(package, options) or []:
        dep = str(dep)
        if dep.startswith("/Game/") and dep not in used:
            stack.append(dep)

all_assets = registry.get_assets_by_path("/Game", recursive=True)
packages = {}
for asset in all_assets:
    package = str(asset.package_name)
    if package.startswith("/Game/__External"):
        continue
    packages.setdefault(package, str(asset.asset_class_path.asset_name))


def file_size(package):
    rel = package[len("/Game/"):]
    for ext in (".uasset", ".umap"):
        path = os.path.join(content_dir, rel + ext)
        if os.path.exists(path):
            return os.path.getsize(path)
    return 0


def folder(package, depth):
    parts = package[len("/Game/"):].split("/")[:-1]
    return "/".join(parts[:depth]) if parts else "(root)"


stats = {}
for package, cls in packages.items():
    size = file_size(package)
    for depth in (1, 2):
        key = (depth, folder(package, depth))
        s = stats.setdefault(key, [0, 0, 0, 0])  # used n, used bytes, unused n, unused bytes
        if package in used:
            s[0] += 1
            s[1] += size
        else:
            s[2] += 1
            s[3] += size

MB = 1024.0 * 1024.0
lines = ["# Content inventory", "",
         "Used = reachable from the game's maps, the character, and assets named in config / C++ (hard and soft references).",
         "", "| Folder | Used assets | Used MB | Unused assets | Unused MB |", "|---|---:|---:|---:|---:|"]
top = sorted([k for k in stats if k[0] == 1], key=lambda k: -(stats[k][1] + stats[k][3]))
for key in top:
    s = stats[key]
    lines.append("| **%s** | %d | %.0f | %d | %.0f |" % (key[1], s[0], s[1] / MB, s[2], s[3] / MB))
    subs = sorted([k for k in stats if k[0] == 2 and k[1].split("/")[0] == key[1] and k[1] != key[1]], key=lambda k: -(stats[k][1] + stats[k][3]))
    for sub in subs:
        t = stats[sub]
        lines.append("| &nbsp;&nbsp;%s | %d | %.0f | %d | %.0f |" % (sub[1], t[0], t[1] / MB, t[2], t[3] / MB))

saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
with open(os.path.join(saved, "ContentInventory.md"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
with open(os.path.join(saved, "ContentInventory_Unused.txt"), "w", encoding="utf-8") as f:
    for package in sorted(p for p in packages if p not in used):
        f.write("%s\t%s\t%.2f MB\n" % (package, packages[package], file_size(package) / MB))
with open(os.path.join(saved, "ContentInventory_Used.txt"), "w", encoding="utf-8") as f:
    for package in sorted(p for p in packages if p in used):
        f.write("%s\t%s\t%.2f MB\n" % (package, packages[package], file_size(package) / MB))
unreal.log_warning("[TH Inventory] %d packages, %d used. Written to %s" % (len(packages), len([p for p in packages if p in used]), saved))

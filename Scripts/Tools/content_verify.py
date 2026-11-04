"""Content check after moves / deletes: lists leftover redirectors and every reference to a package that no longer exists.
Changes nothing. Writes Saved/ContentVerify.txt."""
import os
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)


def exists(package):
    rel = package[len("/Game/"):]
    return any(os.path.exists(os.path.join(content_dir, rel + ext)) for ext in (".uasset", ".umap"))


lines = []
redirectors = []
broken = {}
for asset in registry.get_assets_by_path("/Game", recursive=True):
    package = str(asset.package_name)
    if str(asset.asset_class_path.asset_name) == "ObjectRedirector":
        redirectors.append(package)
    for dep in registry.get_dependencies(package, options) or []:
        dep = str(dep)
        if dep.startswith("/Game/") and not exists(dep):
            broken.setdefault(package, set()).add(dep)

lines.append("Redirectors: %d" % len(redirectors))
lines += ["  " + r for r in sorted(redirectors)]
lines.append("Packages with missing references: %d" % len(broken))
for package, deps in sorted(broken.items()):
    lines.append("  %s -> %s" % (package, ", ".join(sorted(deps))))
saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
with open(os.path.join(saved, "ContentVerify.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
unreal.log_warning("[TH Verify] %d redirectors, %d packages with missing references" % (len(redirectors), len(broken)))

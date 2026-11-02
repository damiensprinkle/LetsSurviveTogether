import unreal, os, re
def out(m): unreal.log_warning("[TH Foley] " + str(m))
eal = unreal.EditorAssetLibrary
OUT = os.path.join(unreal.Paths.project_saved_dir(), "FoleyExport")
os.makedirs(OUT, exist_ok=True)
pat = re.compile(r"(Pistol|Rifle)_(PullOutTheClip|Insert_The_Clip|Shutter)_\d+")
n = 0
for p in eal.list_assets("/Game/WeaponsFoleySFX/SoundWav", recursive=True):
    a = eal.load_asset(p)
    if not isinstance(a, unreal.SoundWave) or not pat.search(a.get_name()):
        continue
    t = unreal.AssetExportTask()
    t.object = a; t.filename = os.path.join(OUT, a.get_name() + ".wav")
    t.automated = True; t.prompt = False; t.replace_identical = True
    t.exporter = unreal.SoundExporterWAV()
    ok = unreal.Exporter.run_asset_export_task(t)
    n += 1
    if not ok: out(f"export failed {a.get_name()}")
out(f"exported {n}; cue example: {[x for x in eal.list_assets('/Game/WeaponsFoleySFX/SoundCue')][:1]}")

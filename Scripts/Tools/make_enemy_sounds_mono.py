"""
Enemy sounds in mono, for 3D audio (2026-11-16). The Steam Audio plugin renders binaurally (HRTF: above, below, behind on
headphones) only one-channel sounds; a stereo sound is just panned. This finds every sound wave the enemies use (whatever
/Game/Enemies references, through cues and data assets) and re-imports each stereo one in place as mono: exported to a
wav, the channels averaged, imported over the same asset (its settings and everything pointing at it are kept). Also sets
SA_EnemyVoice and SA_EnemyFootsteps to Binaural. Re-run after importing new enemy sounds. Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/make_enemy_sounds_mono.py [list]" -nullrhi -unattended
"list" only reports what would change.
"""
import os
import sys
import wave
import unreal

ROOTS = ['/Game/Enemies']
ATTENUATIONS = ['/Game/Audio/SA_EnemyVoice', '/Game/Audio/SA_EnemyFootsteps']
WORK = os.path.join(unreal.Paths.project_saved_dir(), 'MonoSounds')
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
tools = unreal.AssetToolsHelpers.get_asset_tools()
LIST_ONLY = 'list' in ' '.join(sys.argv[1:]).lower()


def out(msg):
    unreal.log_warning('[TH mono] ' + str(msg))


def enemy_sound_waves():
    """Every SoundWave the enemy folder's assets point at, directly or through a sound cue. Not through other Blueprints
    (an enemy referencing the player's Blueprint would pull in the guns and the flashlight, which stay stereo)."""
    options = unreal.AssetRegistryDependencyOptions(True, True, False, False, False)
    seen, waves = set(), []
    stack = [str(a.package_name) for root in ROOTS for a in registry.get_assets_by_path(root, recursive=True)]
    while stack:
        package = stack.pop()
        if package in seen or not package.startswith('/Game/'):
            continue
        seen.add(package)
        classes = [str(d.asset_class_path.asset_name) for d in registry.get_assets_by_package_name(package)]
        if 'SoundWave' in classes:
            waves.append(package)
            continue
        if not (any(package.startswith(root + '/') for root in ROOTS) or 'SoundCue' in classes):
            continue
        deps = registry.get_dependencies(package, options) or []
        stack.extend(str(d) for d in deps)
    return sorted(set(waves))


def to_mono(path):
    """Averages the channels of a 16- or 24-bit wav; returns the mono file's path."""
    with wave.open(path, 'rb') as source:
        channels, width, rate = source.getnchannels(), source.getsampwidth(), source.getframerate()
        frames = source.readframes(source.getnframes())
    if width not in (2, 3):
        raise RuntimeError('%d-byte samples not handled' % width)
    frame = channels * width
    mono = bytearray(len(frames) // channels)
    for i in range(len(frames) // frame):
        total = 0
        for c in range(channels):
            at = i * frame + c * width
            total += int.from_bytes(frames[at:at + width], 'little', signed=True)
        mono[i * width:(i + 1) * width] = int(total / channels).to_bytes(width, 'little', signed=True)
    result = path[:-4] + '_mono.wav'
    with wave.open(result, 'wb') as target:
        target.setnchannels(1)
        target.setsampwidth(width)
        target.setframerate(rate)
        target.writeframes(bytes(mono))
    return result


def remake(package):
    sound = eal.load_asset(package)
    name = sound.get_name()
    task = unreal.AssetExportTask()
    task.object = sound
    task.filename = os.path.join(WORK, name + '.wav')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.SoundExporterWAV()
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('could not export ' + package)
    keep = {}
    for prop in ('attenuation_settings', 'sound_class_object', 'volume', 'pitch', 'looping', 'concurrency_set'):
        try:
            keep[prop] = sound.get_editor_property(prop)
        except Exception:
            pass
    imp = unreal.AssetImportTask()
    imp.filename = to_mono(task.filename)
    imp.destination_path = package.rsplit('/', 1)[0]
    imp.destination_name = name
    imp.automated = True
    imp.replace_existing = True
    imp.save = False
    tools.import_asset_tasks([imp])
    sound = eal.load_asset(package)
    for prop, value in keep.items():
        try:
            sound.set_editor_property(prop, value)
        except Exception:
            pass
    # The import over the asset keeps the old stereo wave in the package as SoundWave_0 (seen 2026-11-16): remove it
    for data in registry.get_assets_by_package_name(package):
        if str(data.asset_class_path.asset_name) == 'SoundWave' and str(data.asset_name) != name:
            leftover = unreal.load_object(None, '%s.%s' % (package, data.asset_name))
            if leftover and leftover != sound and not eal.delete_loaded_asset(leftover):
                out('could not remove the old wave %s.%s' % (package, data.asset_name))
    eal.save_loaded_asset(sound, False)
    return sound.get_editor_property('num_channels')


def main():
    os.makedirs(WORK, exist_ok=True)
    stereo = []
    for package in enemy_sound_waves():
        sound = eal.load_asset(package)
        channels = sound.get_editor_property('num_channels') if sound else 0
        if channels > 1:
            stereo.append(package)
        out('%d ch  %s' % (channels, package))
    out('%d stereo of the enemy sounds' % len(stereo))
    if LIST_ONLY:
        return
    for package in stereo:
        try:
            out('%s -> %d ch' % (package, remake(package)))
        except Exception as e:
            out('FAILED %s: %r' % (package, e))
    for path in ATTENUATIONS:
        attenuation = eal.load_asset(path)
        settings = attenuation.get_editor_property('attenuation')
        settings.set_editor_property('spatialization_algorithm', unreal.SoundSpatializationAlgorithm.SPATIALIZATION_HRTF)
        attenuation.set_editor_property('attenuation', settings)
        eal.save_loaded_asset(attenuation, False)
        out('%s: binaural' % path)


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

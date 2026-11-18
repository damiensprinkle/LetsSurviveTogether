"""
The enemies' own sounds (developer, 2026-11-14), from freesound.org downloads in ../NpcAudios (each with its licence
file; THIRD_PARTY_ASSETS.csv). Imports them into /Game/Audio/Enemies and puts each where it goes:

- AmbusherShriek          DA_Ambusher -> Alert Sounds (only the one that springs first shrieks; in code)
- HunterScream            DA_Hunter -> Scream Sounds (as a chase starts and it calls other hunters in)
- DoorBang                DA_Hunter, DA_Ambusher -> Door Bang Sounds (banging on a closed door)
- ChargerDoorSlam         DA_Charger -> Door Slam Sounds (slamming a door open as it charges)
- ChargerStomp_01..04     DA_MutantLooks -> Footstep Sounds: chargerFootsteps.mp3 is a sequence of four stomps, cut here
                          into one file each (0.5 s from each stomp, the last 0.2 s faded out)
- DismembermentBody       BP_Zombie -> Sever Sounds (an arm or leg coming off)
- DismembermentHead       BP_Zombie -> Head Sever Sounds
- LightCrackle            BP_FirstPersonCharacter -> Flicker Sounds (a stalker near: the player's light crackles)

The enemy sounds are heard in the world like an enemy's voice (SA_EnemyVoice; the stomps SA_EnemyFootsteps); the crackle
plays at the player's own light. The DA_Hunter/DA_Ambusher/DA_Charger settings are also in setup_tactics.py, which loads
these assets: run this first. The stomps are cut in mono; for 3D audio the other stereo files need make_enemy_sounds_mono.py
afterwards (re-importing them here brings back their stereo). Headless, editor closed:
  UnrealEditor-Cmd.exe "<abs>\\LetsSurviveTogether.uproject" -run=pythonscript -script="<abs>/Scripts/Tools/import_npc_audio.py" -nullrhi -unattended
"""
import os
import wave
import unreal

SOURCE = r'C:\Users\Damien Sprinkle\Documents\Unreal Projects\NpcAudios'
DEST = '/Game/Audio/Enemies'
VOICE = '/Game/Audio/SA_EnemyVoice'
STEPS = '/Game/Audio/SA_EnemyFootsteps'
FILES = {
    # asset name: (file, attenuation)
    'AmbusherShriek': ('ambusherShriek.wav', VOICE),
    'HunterScream': ('HunterScream.wav', VOICE),
    'DoorBang': ('DoorBang.wav', VOICE),
    'ChargerDoorSlam': ('ChargerDoorSlam.wav', VOICE),
    'DismembermentBody': ('dismemberment_body.wav', None),
    'DismembermentHead': ('dismemberment_head.wav', None),
    'LightCrackle': ('lightCrackle.wav', None),
}
STOMPS = 'chargerFootsteps.mp3'
STOMP_STARTS = [0.0, 2.09, 4.14, 6.24]   # s: each stomp's onset in the sequence (measured, 2026-11-14)
# Short (developer, 2026-11-16): at a run, 1.5 s stomps overlapped three or four deep and didn't line up with the feet
STOMP_LENGTH = 0.5
STOMP_FADE = 0.2
# Played higher (developer, 2026-11-16: too deep, heavier than the charger looks; then still a little bassy): DA_MutantLooks → Footstep Pitch
STOMP_PITCH = 1.45
WORK = os.path.join(unreal.Paths.project_saved_dir(), 'NpcAudio')
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def out(msg):
    unreal.log_warning('[TH npc audio] ' + str(msg))


def import_sound(path, name, folder=DEST):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        raise RuntimeError('import failed: ' + path)
    return unreal.load_asset(paths[0])


def cut_stomps():
    """The stomp sequence (an mp3: in through Unreal, out again as a wav) cut into one wav per stomp."""
    os.makedirs(WORK, exist_ok=True)
    staging = import_sound(os.path.join(SOURCE, STOMPS), 'ChargerStompSequence', DEST + '/Staging')
    task = unreal.AssetExportTask()
    task.object = staging
    task.filename = os.path.join(WORK, 'sequence.wav')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.SoundExporterWAV()
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('could not export the stomp sequence')
    eal.delete_directory(DEST + '/Staging')
    pieces = []
    with wave.open(task.filename, 'rb') as source:
        channels, width, rate = source.getnchannels(), source.getsampwidth(), source.getframerate()
        frames = source.readframes(source.getnframes())
    if width != 2:
        raise RuntimeError('expected 16-bit audio, got %d bytes a sample' % width)
    # Mono, for 3D audio (only one-channel sounds are rendered binaurally): the channels averaged
    frame = channels * width
    mono = bytearray(len(frames) // channels)
    for i in range(len(frames) // frame):
        total = sum(int.from_bytes(frames[i * frame + c * 2:i * frame + c * 2 + 2], 'little', signed=True) for c in range(channels))
        mono[i * 2:i * 2 + 2] = int(total / channels).to_bytes(2, 'little', signed=True)
    for index, start in enumerate(STOMP_STARTS, 1):
        first = int(start * rate)
        count = int(STOMP_LENGTH * rate)
        chunk = mono[first * 2:(first + count) * 2]
        count = len(chunk) // 2
        fade = int(STOMP_FADE * rate)
        for i in range(max(count - fade, 0), count):
            gain = (count - i) / float(fade)
            value = int.from_bytes(chunk[i * 2:i * 2 + 2], 'little', signed=True)
            chunk[i * 2:i * 2 + 2] = int(value * gain).to_bytes(2, 'little', signed=True)
        path = os.path.join(WORK, 'ChargerStomp_%02d.wav' % index)
        with wave.open(path, 'wb') as piece:
            piece.setnchannels(1)
            piece.setsampwidth(width)
            piece.setframerate(rate)
            piece.writeframes(bytes(chunk))
        pieces.append(path)
    return pieces


def set_attenuation(sound, path):
    if path:
        sound.set_editor_property('attenuation_settings', eal.load_asset(path))
        eal.save_loaded_asset(sound, False)


def main():
    sounds = {}
    for name, (file_name, attenuation) in FILES.items():
        sounds[name] = import_sound(os.path.join(SOURCE, file_name), name)
        set_attenuation(sounds[name], attenuation)
    stomps = []
    for path in cut_stomps():
        stomp = import_sound(path, os.path.splitext(os.path.basename(path))[0])
        set_attenuation(stomp, STEPS)
        stomps.append(stomp)
    out('imported: %s, %d stomps' % (', '.join('%s %.1f s' % (n, s.get_editor_property('duration')) for n, s in sounds.items()), len(stomps)))

    # The ambusher's shriek (the hunter's scream, the door bangs and the charger's door slam: setup_tactics.py)
    ambusher = eal.load_asset('/Game/Enemies/Behaviors/DA_Ambusher')
    ambusher.set_editor_property('alert_sounds', [sounds['AmbusherShriek']])
    eal.save_loaded_asset(ambusher, False)

    mutants = eal.load_asset('/Game/Enemies/Appearance/DA_MutantLooks')
    mutants.set_editor_property('footstep_sounds', stomps)
    mutants.set_editor_property('footstep_pitch', STOMP_PITCH)
    eal.save_loaded_asset(mutants, False)

    zombie = eal.load_asset('/Game/Enemies/BP_Zombie')
    cdo = unreal.get_default_object(zombie.generated_class())
    cdo.set_editor_property('sever_sounds', [sounds['DismembermentBody']])
    cdo.set_editor_property('head_sever_sounds', [sounds['DismembermentHead']])
    eal.save_loaded_asset(zombie, False)

    player = eal.load_asset('/Game/Blueprint/Character/BP_FirstPersonCharacter')
    pdo = unreal.get_default_object(player.generated_class())
    pdo.set_editor_property('flicker_sounds', [sounds['LightCrackle']])
    eal.save_loaded_asset(player, False)
    out('DA_Ambusher alert, DA_MutantLooks footsteps, BP_Zombie sever and head sever, BP_FirstPersonCharacter flicker: set')


try:
    main()
except Exception as e:
    out('ERROR %r' % e)

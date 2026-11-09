import unreal
def out(m): unreal.log_warning("[TH FN] " + str(m))
eal = unreal.EditorAssetLibrary
WAVS = ["/Game/ThirdParty/WeaponsFoleySFX/SoundWav/", "/Game/ThirdParty/FreeWeaponSFX/Wav/"]
att = eal.load_asset("/Game/Audio/FootStep/FootstepSound/Sounds/FootstepsSoundAttenuation")
cls = unreal.load_class(None, "/Script/TacticalHorror.THAnimNotify_WeaponSound")

def wave(name):
    for w in WAVS:
        for p in eal.list_assets(w, recursive=True):
            if p.split(".")[-1] == name:
                return eal.load_asset(p)
    raise RuntimeError("missing " + name)

# (event time in the animation, clip, ms into the clip where the hit we align is, volume, pitch)
# Reload volumes were 1.0 (slide lock 0.9) until the developer turned reload sounds down 10% on 2026-11-09.
# Hit times are from wav_hits.ps1. The G17 uses the FreeWeaponSFX handgun set (its gunshots too, below).
# G17 bones (relative to main_j): mag2_j is the NEW magazine, it comes in from off-screen and seats at 1.46-1.48 s.
# mag1_j is the old one: it leaves the grip at ~0.66 s, and at ~1.9-2.0 s it is moved off-screen (not a sound event).
# Draw: foley right as the draw starts (a notify at exactly 0 can be skipped, hence 0.02).
plans = {
    "/Game/Weapons/M4/Animations/A_FP_M4_Equip": [
        (0.02, "sfx_wpn_ar_foley_aim_01", 0, 1.0, 1.0)],
    "/Game/Weapons/G17/Animations/A_FP_G17_Equip": [
        (0.02, "sfx_wpn_hgn_foley_aim_01", 0, 1.0, 1.0)],
    "/Game/Weapons/M4/Animations/A_FP_M4_Reload": [
        (0.83, "Rifle_PullOutTheClip_02_Wav", 110, 0.9, 1.0),
        (2.00, "Rifle_Insert_The_Clip_06_Wav", 150, 0.9, 1.0)],
    "/Game/Weapons/M4/Animations/A_FP_M4_Reload_Empty": [
        (0.85, "Rifle_PullOutTheClip_02_Wav", 110, 0.9, 1.0),
        (2.02, "Rifle_Insert_The_Clip_06_Wav", 150, 0.9, 1.0),
        (2.62, "Rifle_Shutter_07_Wav", 350, 0.9, 1.0)],
    "/Game/Weapons/G17/Animations/A_FP_G17_Reload": [
        (0.66, "sfx_wpn_hgn_reload_02_magOut", 80, 0.9, 1.0),
        (1.46, "sfx_wpn_hgn_reload_04_magIn", 100, 0.9, 1.0)],
    "/Game/Weapons/G17/Animations/A_FP_G17_Reload_Empty": [
        (0.66, "sfx_wpn_hgn_reload_02_magOut", 80, 0.9, 1.0),
        (1.46, "sfx_wpn_hgn_reload_04_magIn", 100, 0.9, 1.0),
        (1.91, "sfx_wpn_hgn_reload_01_slideLock", 150, 0.81, 1.0)],
}
TRACK = "WeaponSound"
for path, events in plans.items():
    a = eal.load_asset(path)
    if TRACK in [str(n) for n in unreal.AnimationLibrary.get_animation_notify_track_names(a)]:
        unreal.AnimationLibrary.remove_animation_notify_track(a, TRACK)
    unreal.AnimationLibrary.add_animation_notify_track(a, TRACK)
    for t, clip, hit_ms, vol, pitch in events:
        start = max(0.0, t - hit_ms / 1000.0 / pitch)  # higher pitch plays faster, so the hit comes sooner
        n = unreal.AnimationLibrary.add_animation_notify_event(a, TRACK, start, cls)
        n.set_editor_property("sound", wave(clip)); n.set_editor_property("volume_multiplier", vol)
        n.set_editor_property("pitch_multiplier", pitch); n.set_editor_property("attenuation", att)
        out(f"{a.get_name()}: {clip} at {start:.3f}s (hit at {t:.2f}s, pitch {pitch})")
    out(f"{a.get_name()} saved={eal.save_loaded_asset(a, False)}")

da = eal.load_asset("/Game/Weapons/Data/DA_G17")
da.set_editor_property("fire_sounds", [wave(f"sfx_wpn_hgn_gunshot_0{i}") for i in (1, 2, 3)])
for d in (da, eal.load_asset("/Game/Weapons/Data/DA_M4")):
    rs = d.get_editor_property("reload_sound")
    out(f"{d.get_name()} reload_sound={rs.get_path_name() if rs else None} (plays at reload start, on top of the notifies)")
# Barrel bore measured from the mesh: 1.6 cm ring centred 2.63 cm above Main_j, front face at 12.72 cm
da.set_editor_property("muzzle_offset", unreal.Vector(0.0, -2.63, 13.0))
out(f"DA_G17 muzzle {da.get_editor_property('muzzle_offset')} saved={eal.save_loaded_asset(da, False)}")

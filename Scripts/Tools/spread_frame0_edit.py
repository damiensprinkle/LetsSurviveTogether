"""Spreads the change keyed on an animation's first frame over every frame, and optionally onto other animations.

The change is measured per bone (local rotation and translation) between ORIGINAL's frame 0 (the untouched version,
pulled from git into /Game/TempOrigIdle) and EDITED's frame 0. It is then:
  - applied to every frame of ORIGINAL and written into EDITED (so a looping animation keeps the edit), and
  - added to every frame of each animation in ALSO (e.g. the matching walks), on top of their own motion.
REPORT_ONLY animations are compared with their originals and the changes listed, nothing written.

Back up the files first. Run headless with the editor closed (see README.md), then delete Content/TempOrigIdle.
"""
import unreal

RIFLE = "/Game/Animations/ThirdPerson/M4/"
EDITED = RIFLE + "A_TP_M4_Lowered_Idle"
ORIGINAL = "/Game/TempOrigIdle/A_TP_M4_Lowered_Idle"
ALSO = [RIFLE + "A_TP_M4_Lowered_Walk_F", RIFLE + "A_TP_M4_Lowered_Walk_B",
        RIFLE + "A_TP_M4_Lowered_Walk_L", RIFLE + "A_TP_M4_Lowered_Walk_R"]
REPORT_ONLY = [(RIFLE + "A_TP_M4_Raised_Idle", "/Game/TempOrigIdle/A_TP_M4_Raised_Idle")]
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary


def out(msg):
    unreal.log_warning("[TH spread] " + msg)


def frame0_changes(original, edited, label):
    tracks = [str(n) for n in AL.get_animation_track_names(original)]
    o0 = AL.get_bone_poses_for_frame(original, tracks, 0, False)
    e0 = AL.get_bone_poses_for_frame(edited, tracks, 0, False)
    changes = []
    for i, bone in enumerate(tracks):
        dq = e0[i].rotation * o0[i].rotation.inversed()
        angle = abs(dq.get_angle())
        dt = e0[i].translation - o0[i].translation
        if angle > 0.002 or dt.length() > 0.01:
            changes.append((bone, dq, dt))
            out("%s changed %s: %.1f deg, %.2f cm" % (label, bone, unreal.MathLibrary.radians_to_degrees(angle), dt.length()))
    if not changes:
        out("%s: no frame-0 changes" % label)
    return changes


def write(target, source, changes):
    """Writes source's frames plus the changes into target."""
    frames = AL.get_num_frames(source)
    tracks = set(str(n) for n in AL.get_animation_track_names(source))
    todo = [c for c in changes if c[0] in tracks]
    names = [c[0] for c in todo]
    per_frame = [AL.get_bone_poses_for_frame(source, names, f, False) for f in range(frames)]
    controller = target.get_editor_property("controller")
    controller.open_bracket(unreal.Text("Spread frame-0 edits"), False)
    for i, (bone, dq, dt) in enumerate(todo):
        positions, rotations, scales = [], [], []
        for f in range(frames):
            t = per_frame[f][i]
            positions.append(t.translation + dt)
            rotations.append((dq * t.rotation).normalized())
            scales.append(t.scale3d)
        controller.set_bone_track_keys(unreal.Name(bone), positions, rotations, scales, False)
    controller.close_bracket(False)
    out("%s: wrote %d bones x %d frames, saved=%s" % (target.get_name(), len(todo), frames, eal.save_loaded_asset(target, False)))


for current, orig in REPORT_ONLY:
    a, b = eal.load_asset(orig), eal.load_asset(current)
    if a and b:
        frame0_changes(a, b, "REPORT " + b.get_name())

edited = eal.load_asset(EDITED)
original = eal.load_asset(ORIGINAL)
if not edited or not original:
    out("missing edited=%s original=%s" % (edited, original))
else:
    changes = frame0_changes(original, edited, edited.get_name())
    if changes:
        write(edited, original, changes)
        for path in ALSO:
            anim = eal.load_asset(path)
            if anim:
                # The walk is both source and target: its own frames plus the change
                write(anim, anim, changes)
            else:
                out("missing " + path)

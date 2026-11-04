"""Sets DA_M4's spray pattern to CS2's M4A4, from measured bullet positions.

The measurements are cumulative bullet positions in degrees (yaw = right +, pitch = up +) for bullets 1, 5, 10, ... 30.
A Catmull-Rom curve through them fills in the other bullets (it passes exactly through every measured bullet), then the
per-shot steps are the differences, which is what RecoilPattern stores: bullet N lands at the sum of the first N-1 steps
(ATHWeaponBase: the camera carries ViewKickFraction of it, the bullets the rest, so impacts land on the total).

Edit MEASURED to change the pattern; re-run any time. Prints the resulting positions to check against the source.
"""
import unreal

MEASURED = [  # bullet, yaw, pitch
    (1, 0.000, 0.000),
    (5, 0.310, 3.951),
    (10, -1.665, 7.518),
    (15, 3.547, 7.618),
    (20, 2.559, 8.016),
    (25, -0.203, 8.717),
    (30, -0.991, 9.114),
]


def catmull_rom(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)


def position(bullet, axis):
    pts = [(b, (y, p)[axis]) for b, y, p in MEASURED]
    for k in range(len(pts) - 1):
        b1, v1 = pts[k]
        b2, v2 = pts[k + 1]
        if b1 <= bullet <= b2:
            v0 = pts[k - 1][1] if k > 0 else v1 - (v2 - v1)
            v3 = pts[k + 2][1] if k + 2 < len(pts) else v2 + (v2 - v1)
            return catmull_rom(v0, v1, v2, v3, (bullet - b1) / float(b2 - b1))
    return pts[-1][1]


last = MEASURED[-1][0]
positions = [(position(b, 0), position(b, 1)) for b in range(1, last + 1)]
steps = [unreal.Vector2D(positions[i + 1][0] - positions[i][0], positions[i + 1][1] - positions[i][1]) for i in range(last - 1)]

data = unreal.EditorAssetLibrary.load_asset("/Game/Weapons/Data/DA_M4")
data.set_editor_property("recoil_pattern", steps)
data.set_editor_property("recoil_scale", 1.0)
unreal.EditorAssetLibrary.save_loaded_asset(data)

lines = ["bullet  yaw     pitch"] + ["%5d %7.3f %7.3f" % (b + 1, y, p) for b, (y, p) in enumerate(positions)]
unreal.log_warning("[TH Spray] DA_M4 recoil pattern set (%d steps, scale 1, view kick fraction %.2f, %s rpm)\n%s" % (
    len(steps), data.get_editor_property("view_kick_fraction"), data.get_editor_property("rounds_per_minute"), "\n".join(lines)))

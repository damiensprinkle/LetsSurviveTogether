# Third-Person Animation Guide

A working reference for fixing how characters look **to other players**: body animation, how the weapon sits, hands and fingers. First-person (what you see yourself) is separate and isn't covered here.

---

## 1. How it fits together

Each frame, for every character, four things happen in this order:

1. **Body animation.** The legs walk or crouch, and the upper body plays the held weapon's aiming pose. These come from a *body anim set* data asset (`DA_Body_Rifle`, `DA_Body_Pistol`, `DA_Body_Unarmed`).
2. **Weapon placement.** The gun is placed **from the head**: its grip goes to a point in front of and below the head (the *anchor offset*), pointing where the player aims. This keeps a rifle's stock in the shoulder whatever the arms do.
3. **Hand IK.** Both arms are bent so the hands land on the gun's grips. Each hand **copies the first-person hand** on that gun: palm in the same place, turned the same way, and every finger pointing the same way, so the fingers wrap the grip and the index finger sits where it does in first person. The elbows are forced to point down and slightly out.
4. **Hand pose** (optional, normally empty). If the body set has one, the finger bones are first copied from one frame of an animation. With *Match Rig Hands* on (the default) the copied first-person fingers override it.

During **reload, equip and fire**, the IK fades out and the gun rides the right hand, so it follows the animation. Afterwards it blends back.

| What looks wrong | Where to fix it |
|---|---|
| The whole gun is too far forward, back, high, low, left or right | Weapon data → **Third Person Anchor Offset** |
| A hand is in the wrong place on the gun | Weapon data → **Third Person Right / Left Hand Offset** (nudges it from the copied first-person spot) |
| A wrist is bent or twisted wrong | Weapon data → **Third Person Right / Left Hand Rotation** |
| Fingers are curled wrong | They copy the first-person hands, so first check how the hands look in first person. To pose them yourself instead: untick **Third Person Match Rig Hands** and use **Hand Pose** (section 5) |
| The arms, torso or legs move wrong | Body anim set → animation slots (section 6) |

---

## 2. Where the settings live

**Weapon data:** `Content/Weapons/Data/DA_M4` (rifle) and `DA_G17` (pistol), **Third Person** section.

| Setting | Meaning | Units |
|---|---|---|
| Third Person Anchor Offset | Where the gun's grip sits relative to the head. Moves the whole gun. | cm: X forward, Y right, Z up (relative to the aim) |
| Third Person Right Hand Offset | Moves the right hand on the gun (the gun stays put) | cm, same axes |
| Third Person Left Hand Offset | Moves the left hand on the gun | cm, same axes |
| Third Person Right Hand Rotation | Turns the right hand at the wrist: **Pitch** tips the fingers up (+) or down (−), **Yaw** turns them right or left, **Roll** twists around the barrel | degrees |
| Third Person Left Hand Rotation | Same for the left hand | degrees |
| Third Person Right / Left Shoulder Offset | Moves a shoulder (turns the collarbone). The hand stays on the gun, so a shoulder moved toward it **bends that elbow**. If the arm is fully stretched (the hand target is out of reach), also move the hand back a little with the hand offset X. Rifle left: shoulder `8, 3, 0` with hand X `-8` gives a natural bend. | cm, same axes |
| Third Person Match Rig Hands | On (default): each hand copies the first-person hand (palm, angle, fingers); offsets and rotations above nudge from there. Off: hands keep the body animation's angle and fingers. | on/off |
| Third Person Left Hand Follows Right | On: the left hand keeps the body animation's two-handed grip, relative to the right hand, instead of copying the first-person left hand. Off for both guns now. | on/off |
| Third Person Hidden Bones | Rig bones hidden on the third-person copy. The G17's spare magazine is `Mag1` (it sits at the chest in the held frame). | bone names |
| Third Person Rotation Offset | Tilts the whole gun away from the aim. Rarely needed. | degrees |
| Third Person Anchor Bone | The bone the gun is placed from (`head`) | |

**Body anim sets:** `Content/Characters/BodyAnimSets/DA_Body_Rifle`, `DA_Body_Pistol`, `DA_Body_Unarmed`.

| Section | Settings |
|---|---|
| Locomotion | Idle, Walk Forward/Backward/Left/Right, Crouch Idle, Crouch Forward/Backward/Left/Right, Falling. Empty slots fall back to `DA_Body_Unarmed`. |
| Upper Body | Upper Body Pose (the aiming pose layered on the torso and arms), Aim Offset (looking up and down), **Upper Body Yaw** (turns the torso about the vertical, + = right; the rifle's aiming animations stand bladed ~28° right, so `DA_Body_Rifle` uses −28 to square up; the neck turns back so the head still faces forward). Live: `th.UpperBodyYawTune -5` adds degrees. |
| Actions | Fire, Reload, Equip. **Equip Reveal Fraction** (0.6): the gun stays hidden for that share of the Equip animation and appears in the hand as the draw brings it forward (Lyra's draw otherwise swings the gun behind the shoulder). Raise it if the gun appears too early, lower it if too late. Test off-screen with `th.AutoSwitchAfter 3`. |
| Hand IK | Hand IK Weight (0 = off, 1 = full), arm bone names |
| Lowered | **Lowered Set** (another body set used while the weapon is down: its idle and walks), **Ready Hold Time** (seconds the weapon stays up after the last shot or letting go of aim; rifle 7), **Raise Time** / **Lower Time**. Aiming or shooting raises it. Lowered, the gun sits in the lowered animation's right hand and the left hand is pulled onto the handguard (nudge it with the weapon's Left Hand Offset / Rotation, same as when aiming); to change how it's carried, swap the Lowered Set's animations. Rifle: `DA_Body_Rifle_Lowered` (`AS_F_Rifle_Idle_01`, `AS_F_Rifle_Walk_*`). Pistol: `DA_Body_Pistol_Lowered` (`MM_Pistol_Idle_Hipfire`, retarget of Lyra's UE4 hip-fire idle; walks on the unarmed legs with that pose on top). |
| Hand Pose | **Hand Pose** (animation), **Hand Pose Time** (which frame, in seconds), **Hand Pose Right / Left** (which hands' fingers to copy) |

**Current values (2026-11-03)**

| | Anchor | Hand offsets and rotations | Match rig hands | Left follows right | Hand pose |
|---|---|---|---|---|---|
| Rifle (`DA_M4`) | 18, 5, −12 | all 0 | on | off | none |
| Pistol (`DA_G17`) | 40, 2, −14 | all 0 | on | off | none |

The 2026-11-02 hand offsets (rifle right `3, 1.5, 0` / `15, 0, 0`, left `8, 0, 2`) and the pistol's Lyra hand pose were workarounds for the old method and are cleared.

---

## 3. Console commands

Open the console with **`~`** while playing (Play in the editor).

### Watch yourself from outside

| Command | What it does |
|---|---|
| `th.ThirdPerson 1` | Shows your own character from outside, as other players see it. Your character's facing is frozen while it's on. |
| `th.ThirdPerson 0` | Back to normal first-person view. |
| `th.ThirdPersonView "distance angle height lookheight"` | Moves that camera. **Distance** in cm. **Angle** goes around the character (0 = in front). **Height** is added on top of **lookheight**, the point it looks at above the character's middle. Default `200 60 20 40`. |

Camera presets that worked well:

| View | Command |
|---|---|
| Whole body, front-right | `th.ThirdPersonView 200 60 20 40` |
| Front | `th.ThirdPersonView 130 30 0 55` |
| Rifle, close on one side | `th.ThirdPersonView 110 100 0 55` |
| Rifle, close on the other side | `th.ThirdPersonView 110 -15 0 55` |
| Pistol, close on one side | `th.ThirdPersonView 110 90 0 55` |
| Pistol, close on the other side | `th.ThirdPersonView 110 -60 0 55` |
| Very close | lower the distance, e.g. `80` |

The aiming stances turn the torso, so "left side" and "right side" don't line up exactly with 90 / −90. Change the angle 20–30° at a time until you see the hand you want. Keep the height at `0` for close-ups; a larger height puts the camera above the character, looking down.

### Tune the hold live

**Hand close-ups:** add a body bone name as a fifth value to `th.ThirdPersonView` and the camera looks at that bone instead of the body's centre. The angle is still measured around the character (0 front, 90 its right, −90 its left); keep the look height at 0.

| Command | View |
|---|---|
| `th.ThirdPersonView "35 90 5 0 hand_r"` | Right hand from the right side |
| `th.ThirdPersonView "40 -20 -12 0 hand_l"` | Left hand from the front-left, slightly below |
| `th.ThirdPersonView "40 20 -15 0 hand_l"` | Left hand from the front, below |

`th.DebugHandFingers 0` keeps the body animation's own fingers (the hands are still placed on the gun); `1` (normal) copies the first-person fingers. Use it to compare.

`th.ThirdPersonTune` **adds** numbers to the held weapon's data asset values, live, while you watch. Give up to 21 numbers in this order:

```
th.ThirdPersonTune  anchorX anchorY anchorZ   rightX rightY rightZ   rightPitch rightYaw rightRoll   leftX leftY leftZ   leftPitch leftYaw leftRoll   rightShoulderX rightShoulderY rightShoulderZ   leftShoulderX leftShoulderY leftShoulderZ
```

| Example | Effect |
|---|---|
| `th.ThirdPersonTune -3` | Gun 3 cm back |
| `th.ThirdPersonTune 0 0 2` | Gun 2 cm up |
| `th.ThirdPersonTune 0 0 0  2 0 0` | Right hand 2 cm forward |
| `th.ThirdPersonTune 0 0 0  0 0 0  10` | Right fingers tipped up 10° (straightens a wrist bent down) |
| `th.ThirdPersonTune 0 0 0  0 0 0  0 0 0  5 0 1` | Left hand 5 cm forward, 1 cm up |
| `th.ThirdPersonTune 0 0 0  0 0 0  0 0 0  0 0 0  0 0 20` | Left hand twisted 20° around the barrel |
| `th.ThirdPersonTune 0 0 0  0 0 0  0 0 0  -8 0 0  0 0 0  0 0 0  8 3 0` | Left hand 8 cm back, left shoulder 8 cm forward and 3 cm in: bends the left elbow |
| `th.ThirdPersonTune ""` | Clear it, back to the data asset values |

It applies to whichever weapon you're holding. Press 1 or 2 to switch weapons, but clear or retype it when you switch, because the numbers aren't per weapon.

When it looks right, **add the numbers to the data asset's values** (section 7), then clear the tune. Otherwise they get added twice.

### Other helpers

| Command | What it does |
|---|---|
| `th.DebugThirdPersonWeapon 1` | Draws the gun's grip-to-muzzle line (red) and the aim direction (green), and logs placement every frame |
| `th.DebugThirdPersonWeapon 2` | The same, plus the gun drawn in plain grey (easier to see against the dark gloves) |
| `th.DebugThirdPersonWeapon 0` | Off |
| `th.AutoWalk 1` / `0` | Walks the character on its own, so you can watch the walk cycle from the outside camera. `th.AutoWalkAngle 90` strafes right, `180` walks backwards. `th.AutoWalk 4` crouches in place. |
| `th.AutoLookPitch 30` | Makes the character look up 30° (negative = down), to check the aim offset |
| `th.AutoReloadAfter 1` | Reloads once after 1 s. Fire a few shots first, because a full magazine won't reload. |
| `th.AutoFireAfter 1` | Holds the trigger from 1 s in (`0` = off) |

---

## 4. Workflow for position and wrist fixes

1. Play, press `~`, run `th.ThirdPerson 1`, then a close camera preset.
2. Change one thing at a time with `th.ThirdPersonTune`, in small steps: **1–3 cm** or **5–10°**.
3. Check from at least two angles (one each side), then from the front.
4. Check it **walking** (`th.AutoWalk 1`), **looking up and down** (`th.AutoLookPitch 40`, then `-40`) and **crouched** (`th.AutoWalk 4`).
5. Make the numbers permanent (section 7).

Fix things in this order, because each one moves the next:
1. **Anchor:** the gun against the body (stock in the shoulder, sights near the eye, nothing through the head).
2. **Right hand offset**, then **right hand rotation** (the wrist).
3. **Left hand offset**, then **left hand rotation**.
4. **Fingers** (section 5).

---

## 5. Posing fingers yourself (Hand Pose)

**This is the recommended way to get natural fingers.** The copied first-person fingers come from a larger hand with different proportions, so they bunch up (2026-11-03). A hand covered by a Hand Pose (**Hand Pose Right / Left** ticked) uses your posed fingers instead of the copied ones. Keep **Third Person Match Rig Hands** on: it still places and turns the hand on the grip.

**See the gun while posing:** in the animation editor's Skeleton Tree, right-click `hand_r` → **Add Preview Asset** → pick the weapon mesh (e.g. `SK_M4` / the G17 mesh). It's only a preview and isn't saved into the animation. Move it into the palm with the preview asset's transform, then pose the fingers around it. For the left hand, preview the gun on `hand_l` the same way, or pose by eye.

The commands above can't bend fingers. Fingers are posed in the animation editor, and the game copies **only the finger bones** from that pose. The arm, wrist and hand position still come from the IK.

### Make the pose

1. In the Content Browser, open the folder of a pose close to what you want:
   - pistol: `Content/Characters/Mannequins/Anims/Pistol/` → `MF_Pistol_Idle_ADS`
   - rifle: `Content/SampleAnimationPack/Animations/Rifle/` → `AS_F_ADS_Rifle_Idle_01`
2. Right-click it → **Duplicate**, and name the copy after the hand, e.g. `AS_Pistol_HandPose`. Never edit the original.
3. Open the copy. Pause it and scrub to frame 0.
4. In the **Skeleton Tree** panel on the left, use the search box to find the finger bones (below). If they're hidden, open the panel's settings (gear or filter icon) and choose **Show All Bones**.
5. Select a bone, press **E** for the rotate tool, and switch the gizmo to **local space** with the globe/cube icon at the top right of the viewport. Drag the ring that bends the joint.
6. Press **Key** in the toolbar (or **S**) to record the change. Do this after each bone you change.
7. **Save** (Ctrl+S).

Optional: to see the bone changes as a standalone animation, use **Create Asset → Create Animation → Current Pose** in the toolbar.

### Finger bone names (UE5 mannequin, right hand; left hand is the same with `_l`)

| Finger | Bones (knuckle → tip) |
|---|---|
| Thumb | `thumb_01_r`, `thumb_02_r`, `thumb_03_r` |
| Index (trigger finger) | `index_metacarpal_r`, `index_01_r`, `index_02_r`, `index_03_r` |
| Middle | `middle_metacarpal_r`, `middle_01_r`, `middle_02_r`, `middle_03_r` |
| Ring | `ring_metacarpal_r`, `ring_01_r`, `ring_02_r`, `ring_03_r` |
| Pinky | `pinky_metacarpal_r`, `pinky_01_r`, `pinky_02_r`, `pinky_03_r` |

Trigger finger in the guard: straighten `index_01_r` a little, then bend `index_02_r` and `index_03_r` slightly, so the finger reaches forward and rests on the trigger. A finger outside the guard (trigger discipline) lies straight along the frame instead.

### Use the pose

1. Open `DA_Body_Pistol` (or `DA_Body_Rifle`) → **Hand Pose** section:
   - **Hand Pose:** your new animation
   - **Hand Pose Time:** `0` (or the second of the frame you posed)
   - **Hand Pose Right:** on (right hand's fingers)
   - **Hand Pose Left:** on if you posed the left hand too, otherwise off
2. Save, Play, `th.ThirdPerson 1`, and look up close.
3. Expect two or three rounds of adjust → look. The editor preview has no gun in the hand, so you're posing a little blind.

**Looping animations (idles, walks):** a key only changes the frame it's on. Every other frame keeps the original, so the change fades out as the animation plays and snaps back at the loop. Use **Hand Pose** for fingers (only frame 0 is read). For arm or body changes to an idle or walk, key frame 0, save, close the editor, and ask Claude to run `Scripts/Tools/spread_frame0_edit.py`, which applies the frame-0 change to every frame. Use **rotate (E)**, not move (W): moving an arm bone stretches the limb.

Changes to data assets and animations usually show up while Play is running. If not, stop and press Play again.

---

## 6. Swapping body animations

To try a different aiming pose or walk:

1. Open `DA_Body_Rifle` / `DA_Body_Pistol`.
2. Change the slot, e.g. **Upper Body Pose** or **Walk Forward**, to another animation for the **UE5 mannequin** skeleton. Good sources in the project:
   - `Content/SampleAnimationPack/Animations/Rifle/`: rifle idles, walks, aims (female, `AS_F_*`)
   - `Content/Characters/Mannequins/Anims/Rifle/` and `/Pistol/`: UE5 template
   - `Content/Blueprint/Lyra_Content/Characters/Mannequin/Animations/`: Lyra (male, `MM_*`). The `Mannequin_UE4` folders are for the old skeleton; avoid them.
3. Play and check. Because the gun and hands are placed by IK, a new pose mostly changes the torso, the elbows' starting point and the hand angles. You may need to retune the hand rotations afterwards.

Walk speed matching is automatic (from each clip's root motion). If a walk slides or looks slow-motion, set **Walk Anim Speed** / **Crouch Anim Speed** on the set (cm/s of the clip).

---

## 7. Making changes permanent

`Scripts/Tools/setup_body_anims.py` **overwrites** these values when it runs: the animation slots and Hand Pose in the `DA_Body_*` sets, and the anchor, hand offsets, rotations, "match rig hands", "left hand follows right" and the G17's hidden bones on `DA_M4` / `DA_G17`.

So after tuning, either:
- **Tell Claude the final numbers and assets**, and it will update the script's tables; or
- Edit the tables yourself: `WEAPONS` near the end of the script (anchor, right, right_rot, left, left_rot as `(X, Y, Z)` / `(Pitch, Yaw, Roll)`) and `SETS` near the top (animation paths, `hand_pose`).

Then put the same values in the data assets (or re-run the script with the editor closed, see `Scripts/Tools/README.md`).

If you used `th.ThirdPersonTune`, **add** its numbers to the asset's current values. For example, an anchor of `18 5 -12` plus a tune of `-2 0 1` = `16 5 -11`.

---

## 8. Checklist before calling it done

For each weapon, from outside (`th.ThirdPerson 1`):

- [ ] Standing still: stock in the shoulder (rifle), arms out (pistol); hands on the grips; fingers wrapped, trigger finger right
- [ ] No clipping: gun vs head, gun vs arms, hands vs magazine
- [ ] Walking in all four directions (`th.AutoWalk 1` + `th.AutoWalkAngle 0 / 90 / 180 / -90`)
- [ ] Crouched, still and walking
- [ ] Looking up and down (`th.AutoLookPitch 40` / `-40`)
- [ ] Firing a burst: gun kicks with the hands
- [ ] Reloading: gun follows the right hand, left hand goes to the magazine, both hands return afterwards
- [ ] Switching weapons (1 / 2)
- [ ] Finally, a real two-player session (Play settings → 2 players, *Play As Listen Server*): look at each other

---

## 9. Known limits

- The rifle animations are **female** on a male body, so some arm angles look slightly off. A male rifle set would help.
- The pistol has no walk set of its own (it uses the unarmed legs with the aiming pose on top).
- There's no turn-in-place: feet slide when turning on the spot.
- Downed and dead have no body animation yet.
- These may be easier in an **Animation Blueprint** later (turn-in-place, downed, hit reactions). The C++ can be set up to feed one; ask when you want it.

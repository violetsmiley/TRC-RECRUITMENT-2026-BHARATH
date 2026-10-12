# Task 1 — Webots project (1a keyboard control + 1b line follower)

One Webots project, two worlds, two controllers. Both worlds are built from
Webots **base nodes only** — there is not a single `EXTERNPROTO` line, so
nothing is downloaded and nothing is looked up inside your Webots installation.

## Folder layout

Webots works out where the controllers live from the world file's parent
folder, so keep this structure intact.

```
task1_webots/
├── worlds/
│   ├── task1a.wbt              <- keyboard control
│   └── task1b.wbt              <- line follower
└── controllers/
    ├── keyboard_controller/
    │   └── keyboard_controller.py
    └── line_follower/
        └── line_follower.py
```

## Running either one

1. Extract the zip somewhere stable. On **Windows use right-click ▸ Extract
   All** — double-clicking the zip shows a preview that only *looks* like a
   folder, and Webots cannot find the controllers inside it.
2. Webots → **File ▸ Open World…** → `worlds/task1a.wbt` or `worlds/task1b.wbt`.
3. Press ▶.
4. For **1a only**: click once inside the 3D view so it takes keyboard focus.
   1b needs no input at all.

If Python fails to start, set **Tools ▸ Preferences ▸ General ▸ Python command**
to `python3` (macOS/Linux) or the full path to `python.exe` (Windows), then
reload the world with Ctrl+Shift+T. Webots ships its own `controller` module —
there is nothing to `pip install`.

---

# Task 1a — keyboard control

| Key       | Action                      |
|-----------|-----------------------------|
| `W` / `↑` | forward                     |
| `S` / `↓` | backward                    |
| `A` / `←` | turn left                   |
| `D` / `→` | turn right                  |
| `SPACE`   | brake — stops immediately   |

Keys combine: `W`+`D` arcs right, `S`+`A` reverse-turns. Releasing lets the
robot roll down smoothly; `SPACE` cuts it dead.

`keyboard.getKey()` returns only **one** key per call, so the controller drains
the buffer every step — that is what makes two keys work together. `& 0xFFFF`
strips the SHIFT/CTRL/ALT bits Webots ORs into the key code. Speeds ramp by
`ACCEL` rad/s per step instead of jumping, so the wheels don't spin out from a
standing start.

---

# Task 1b — line follower

Press ▶ and the robot drives the loop on its own. Same chassis as 1a, with
three downward-facing infra-red sensors added under the nose.

```
        ir_left   ir_center   ir_right        +y is the robot's LEFT
           |          |          |
      --------------------------------  front of chassis
                  travel ->
```

## How the line is detected

A Webots infra-red `DistanceSensor` does **not** report the distance it
measured. It divides that distance by a reflection factor first:

```
f = 0.2 + 0.8 × red_level × (1 − 0.5 × roughness) × (1 − 0.5 × occlusion)
```

So the sensor is really measuring *darkness*, expressed as an inflated
distance. Measured on the running robot:

| Surface | Real distance | Reported |
|---|---|---|
| clear floor | 18 mm | **820 – 990** |
| the line | 16.5 mm | **0** (its inflated distance runs off the table end) |

The lookupTable is `[0 1000 0, 0.120 0 0]`. **Sizing that table is the thing to
get right** — one sized for the real 18 mm gap puts *both* surfaces past the
end of the table, where the sensor returns 0 for everything and the robot is
blind. The controller detects exactly that at startup and says so.

The floor figure is a range, not a number, because the nose pitches and rolls
as the robot drives and its height above the floor keeps changing. That range
matters — see the calibration section.

### The sensors are analog, not yes/no

Each sensor fires **7 rays over a 1.0 rad cone**, which puts a ~20 mm spot on
the floor; Webots averages the rays' responses, so a sensor reports *the
fraction of its spot that is over the line*.

This is not a detail. With a single ray each sensor is a yes/no detector, and
with a narrow line the robot gets no steering signal at all until it has
drifted far enough for an outer sensor to snap fully on. Bang-bang input,
violent weaving, and it eventually throws itself off the track.

Three things work together to keep the signal graded:

* the line is **40 mm** wide;
* the outer sensors sit at **±25 mm**, so their 20 mm spots already clip the
  line edge when the robot is centred — there is no dead zone to cross;
* **7 rays** per sensor turn edge coverage into a proportional reading.

## The control law

```
error = (w_right - w_left) / (w_left + w_centre + w_right)
turn  = KP * error + KD * (error - previous_error)
left  = base + turn        right = base - turn
```

`error` is positive when the line is off to the right, which speeds the left
wheels up and turns the robot right. Base speed is cut by up to 55 % in
proportion to `|error|`, so it slows into corners and opens up on the straights.

If every sensor loses the line, the robot spins towards whichever side saw it
last while creeping forward, until it finds it again.

## Tuning

All at the top of `line_follower.py`:

| Constant          | Default | Effect                                            |
|-------------------|---------|---------------------------------------------------|
| `BASE_SPEED`      | `3.6`   | rad/s on the straights → 0.18 m/s                 |
| `KP`              | `3.4`   | how hard it corrects — too high and it weaves     |
| `KD`              | `7.0`   | damps the weave; raise this before lowering `KP`  |
| `CORNER_SLOWDOWN` | `0.55`  | fraction of speed given up in a hard corner       |
| `LOST_TOTAL`      | `0.18`  | below this the line counts as lost                |
| `DEBUG_EVERY`     | `60`    | steps between console status lines; `0` is silent |

Want it faster? Raise `BASE_SPEED` first, and when it starts cutting corners
raise `KP` and `KD` together.

## What was checked before shipping

The controller is driven round the real track geometry in a head-less
simulation whose sensor model implements Webots' actual reflection formula,
the analog multi-ray footprint, and the floor reading wandering as the nose
pitches. Eighteen scenarios:

* the **first world**, as a regression test: expected to *fail*, and it does —
  both surfaces read 0 and the robot never moves;
* real conditions (line reads 0, floor wanders) on an ideal drive and on a
  skid steer delivering 30 % to 160 % of commanded rotation;
* yaw lag up to 0.3 s, standing in for tyre scrub;
* the robot riding 3 mm lower and 4 mm higher than designed;
* violent pitching — 15 mm of height jitter;
* dim floor, glossy surfaces, matte surfaces, heavy sensor noise;
* shoved 70 mm and 110 mm off the line mid-lap, and shoved off while turned
  35 degrees.

All seventeen working scenarios complete laps, holding within **5–14 mm** of
the line centre. Recovery from a 110 mm shove took 362 control steps.

### Tuning

All at the top of `line_follower.py`:

| Constant | Default | Effect |
|---|---|---|
| `BASE_SPEED` | `2.6` | rad/s on the straights → 0.13 m/s |
| `KP` | `2.8` | how hard it corrects |
| `KD` | `3.0` | damps the weave |
| `ERROR_SMOOTH` | `0.35` | low-pass before the derivative — stops `KD` spiking |
| `DEADBAND` | `0.30` | how much of the scale counts as clean floor |
| `SEARCH_SPIN` | `3.0` | how fast it pivots hunting for a lost line |
| `DEBUG_EVERY` | `60` | steps between console lines; `0` is silent |

Want it faster? Raise `BASE_SPEED` first; when it starts cutting corners,
raise `KP` and `KD` together.

## Useful while debugging

* **View ▸ Optional Rendering ▸ Show DistanceSensor Rays** draws the three
  sensor beams, which makes it obvious when one is off the line.
* The console prints `TRACK L=.. C=.. R=.. err=..` every 60 steps. `L/C/R` are
  the normalised 0..1 values, so a healthy run over a straight reads roughly
  `L=0.00 C=1.00 R=0.00`.
* Ctrl+Shift+T reloads the world and re-runs calibration from the start pose.

---

## Notes on Webots versions

Both worlds target **R2025a** (`#VRML_SIM R2025a utf8`).

PROTO nodes such as `RectangleArena` and `TexturedBackground` resolve through a
URL at load time, and that resolution is the usual cause of a world opening to
red console errors like `Skipped unknown 'TexturedBackground' node or PROTO`.
When that happens the scene loads *without* them, and since the lighting PROTO
is normally the only light source the 3D view goes black even though the robot
is fine. Nothing here depends on that machinery: the floor, walls, sky, lights
and the whole track are plain `Solid` / `Background` / `DirectionalLight` nodes
written straight into the world files.

Running an **older** Webots? Change the header line to your version, e.g.
`#VRML_SIM R2023b utf8`; everything else works unchanged back to R2022b. On
**R2022a or older**, also replace each `Pose { … }` with `Transform { … }` —
the field names are identical.

## Where Task 1b goes next

The chassis already carries wheel encoders (`enc_fl`, `enc_fr`, `enc_rl`,
`enc_rr`), so odometry is available if you want to measure lap times or
distance travelled without touching the world file.

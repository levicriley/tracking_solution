# Synthetic Tracker‑Test Generator  
*`generate_input.py` documentation – Source omitted for brevity*

This tool fabricates paired JSON files to unit‑test any multi‑object
tracker.  The first file (*detections only*) is fed into the tracker,
while the second file (*detections + persistent IDs*) is used as the
ground‑truth for evaluation.

```
tests/input.json     ← what your C++ tracker ingests  
tests/expected.json  ← what the tracker is supposed to output
```

Both files share identical timestamps and bounding boxes; only the ID
field differs.

---

## 1 · High‑Level Workflow

```
for each frame k = 0 … N‑1:
    • update global wind / rotation / zoom
    • add or retire objects to keep #visible within [min_objects, max_objects]
    • move every object via:
          bias   +  shared wind
        + random walk (per‑object)
        + sway         (small Gaussian jitter)
    • inject measurement noise  (pos & size)
    • optionally drop detections for occlusion testing
    • write:
        – tests/input.json   : list of detections {x,y,w,h}
        – tests/expected.json: same list + "id"
```

The generator is **fully deterministic** if you supply a `--seed`
or store one in `defaults.ini`.

---

## 2 · Motion & Noise Components

| Component | Scope | CLI flag(s) | Purpose |
|-----------|-------|-------------|---------|
| **Bias drift**      | Global      | `--bias-x`, `--bias-y`        | Constant camera pan. |
| **Wind**            | Global      | `--wind-sigma`, `--wind-max`  | Bounded random walk added to *all* objects every frame. |
| **Rotation / Zoom** | Global      | `--rot-sigma`, `--scale-sigma`| Small rotation & isotropic zoom about image‑centre. |
| **Random walk**     | Per‑object  | `--step-max`                  | Uniform jitter within a square box. |
| **Sway**            | Per‑object  | `--sway-sigma`                | Tiny Gaussian “leaf rustle”. |
| **Meas. noise pos** | Per‑det     | `--noise-pos`                 | Gaussian noise added **after** real motion. |
| **Meas. noise size**| Per‑det     | `--noise-size`                | Noise on `w` / `h`. |
| **Drop‑outs**       | Per‑object  | `--drop-prob`, `--drop-max`   | Simulates occlusion by hiding detections. |

> **Note**  
> *Real* motion alters both files; measurement noise alters *only*
> `tests/input.json`.

---

## 3 · Field‑By‑Field Schema

```jsonc
[
  {
    "timestamp": "2025-03-24T18:00:00.000000",
    "detections": [ { "x": 0.25, "y": 0.50, "w": 0.12, "h": 0.12 } ],
    "tracks":     [ { "id": 7,   "x": 0.25, "y": 0.50, "w": 0.12, "h": 0.12 } ]
  }
]
```

Coordinates are **normalised (0–1)** and axis‑aligned.

---

## 4 · CLI Cheat‑Sheet (defaults in parentheses)

| Group | Flag | Default | Purpose |
|-------|------|---------|---------|
| Timing | `-f, --frames` | 30 | Total frames |
|        | `--dt-min / --dt-max` | 0.030 / 0.040 | Δt bounds (s) |
| Population | `--min-objects / --max-objects` | 1 / 5 | Visible count range |
|            | `--min-life  / --max-life`      | 5 / 20 | Object lifetime (frames) |
| Size | `--min-size / --max-size` | 0.10 / 0.20 | Box side bounds |
| Global motion | `--bias-x / --bias-y` | 0 | Constant drift |
|               | `--wind-sigma / --wind-max` | 0.02 / 0.10 | Shared wind walk |
|               | `--rot-sigma` | 0 | Rotational σ (rad) |
|               | `--scale-sigma` | 0 | Zoom σ |
| Per‑object motion | `--step-max` | 0 | Random walk radius |
|                   | `--sway-sigma` | 0.01 | Sway σ |
| Noise | `--noise-pos / --noise-size` | 0.002 / 0.001 | Measurement σ |
| Drop‑outs | `--drop-prob / --drop-max` | 0.05 / 3 | Occlusion model |
| Misc | `--seed` | 0 | RNG seed |

---

## 5 · Integration Example

```bash
python tests/generate_input.py --frames 60 --rot-sigma 0.002
tracking-solution \
    --input tests/input.json \
    --output tests/output.json \
    --vis-dir tests/vis
python tests/compare_tracks.py \
    --expected tests/expected.json \
    --output   tests/output.json \
    --iou-thresh 0.90
```

---

## 6 · Changelog

| Version | Notes |
|---------|-------|
| **1.3** | Added rotation & zoom (`--rot-sigma`, `--scale-sigma`) |
| **1.2** | Drop‑out bursts up to `--drop-max` |
| **1.1** | Split measurement noise into *pos* vs *size* |
| **1.0** | Initial release |

---

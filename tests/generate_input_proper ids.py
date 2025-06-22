#!/usr/bin/env python3
"""
Synthetic tracker-test generator with:

• Variable frame spacing (Δt ∈ [dt_min, dt_max])
• Configurable object counts & lifetimes
• Random-walk + drift motion
• Gaussian position / size jitter
• Optional drop-outs (objects vanish 1…drop_max frames)

Run  ──  python tests/generate_input.py -h   for full CLI.
"""

import json, argparse, random, datetime as dt
from pathlib import Path
from typing import List, Dict, Any

# ---------------------------------------------------------------------------
def clamp(val: float, lo: float = 0.0, hi: float = 1.0) -> float:
    """Keep val inside [lo, hi]."""
    return max(lo, min(val, hi))


# ---------------------------------------------------------------------------
def main() -> None:
    p = argparse.ArgumentParser(
        description="Synthetic test-data generator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    # sequence length & timing
    p.add_argument("-f", "--frames", type=int, default=30,
                   help="total number of frames")
    p.add_argument("--dt-min", type=float, default=0.030,
                   help="minimum inter-frame interval (s)")
    p.add_argument("--dt-max", type=float, default=0.040,
                   help="maximum inter-frame interval (s)")

    # object population
    p.add_argument("--min-objects", type=int, default=1,
                   help="minimum objects visible in any frame")
    p.add_argument("--max-objects", type=int, default=5,
                   help="maximum objects visible in any frame")
    p.add_argument("--min-life", type=int, default=5,
                   help="minimum lifetime (frames) of an object")
    p.add_argument("--max-life", type=int, default=20,
                   help="maximum lifetime (frames) of an object")

    # motion & noise
    p.add_argument("--step-max",  type=float, default=0.04,
                   help="max per-frame centre displacement (normalised units)")
    p.add_argument("--bias-x",    type=float, default=0.0,
                   help="constant drift in x per frame")
    p.add_argument("--bias-y",    type=float, default=0.0,
                   help="constant drift in y per frame")
    p.add_argument("--noise-pos", type=float, default=0.002,
                   help="σ of positional (x,y) Gaussian noise")
    p.add_argument("--noise-size", type=float, default=0.001,
                   help="σ of width/height Gaussian noise")

    # drop-out behaviour
    p.add_argument("--drop-prob", type=float, default=0.10,
                   help="per-frame probability a *visible* object begins drop-out")
    p.add_argument("--drop-max",  type=int,   default=3,
                   help="maximum consecutive frames an object is dropped")

    p.add_argument("--seed", type=int, default=0,
                   help="RNG seed (reproducible runs)")

    args = p.parse_args()
    rng = random.Random(args.seed)

    Path("tests").mkdir(exist_ok=True)

    # -----------------------------------------------------------------------
    # Spawn objects (with lifetimes) on the fly
    # -----------------------------------------------------------------------
    objects: List[Dict[str, Any]] = []       # active + future objects
    next_id = 0

    for frame in range(args.frames):
        # ensure current active object count is within [min_objects, max_objects]
        active = [o for o in objects if o["start"] <= frame < o["end"]]
        need   = rng.randint(args.min_objects, args.max_objects) - len(active)

        for _ in range(max(0, need)):
            life = rng.randint(args.min_life, args.max_life)
            objects.append({
                "id": next_id,
                "start": frame,
                "end":   min(frame + life, args.frames),
                "x": rng.uniform(0.1, 0.8),
                "y": rng.uniform(0.1, 0.8),
                "drop_remaining": 0,      # frames left to stay invisible
                "seen_once": False,       # becomes True after first visible frame
            })
            next_id += 1

    # -----------------------------------------------------------------------
    # Generate timestamps with variable Δt
    # -----------------------------------------------------------------------
    ts_list = [dt.datetime(2025, 3, 24, 18, 0, 0)]
    for _ in range(1, args.frames):
        delta = rng.uniform(args.dt_min, args.dt_max)
        ts_list.append(ts_list[-1] + dt.timedelta(seconds=delta))

    frames_json, expected_json = [], []

    # -----------------------------------------------------------------------
    # Simulate each frame
    # -----------------------------------------------------------------------
    for k, ts in enumerate(ts_list):
        ts_str = ts.strftime("%Y-%m-%dT%H:%M:%S.%f")
        detections, tracks = [], []

        for obj in objects:
            if not (obj["start"] <= k < obj["end"]):
                continue  # not alive yet / already dead

            # ---------- drop-out logic ----------
            if obj["drop_remaining"] == 0 and obj["seen_once"]:
                if rng.random() < args.drop_prob:
                    obj["drop_remaining"] = rng.randint(1, args.drop_max)

            dropped = obj["drop_remaining"] > 0
            if dropped:
                obj["drop_remaining"] -= 1

            # ---------- latent motion ----------
            obj["x"] = clamp(obj["x"] +
                             rng.uniform(-args.step_max, args.step_max) +
                             args.bias_x)
            obj["y"] = clamp(obj["y"] +
                             rng.uniform(-args.step_max, args.step_max) +
                             args.bias_y)

            # ---------- measurement (only if not dropped) ----------
            if not dropped:
                mx = clamp(obj["x"] + rng.gauss(0, args.noise_pos))
                my = clamp(obj["y"] + rng.gauss(0, args.noise_pos))
                w  = clamp(0.12 + rng.gauss(0, args.noise_size), hi=0.5)
                h  = clamp(0.12 + rng.gauss(0, args.noise_size), hi=0.5)

                det = {"x": mx, "y": my, "w": w, "h": h}
                detections.append(det)
                tracks.append({"id": obj["id"], **det})
                obj["seen_once"] = True  # first visible frame registered

        frames_json.append({"timestamp": ts_str,
                            "detections": detections})
        expected_json.append({"timestamp": ts_str,
                              "tracks": tracks})

    # -----------------------------------------------------------------------
    # Save
    # -----------------------------------------------------------------------
    with open("tests/input.json", "w") as f:
        json.dump(frames_json, f, indent=2)
    with open("tests/expected.json", "w") as f:
        json.dump(expected_json, f, indent=2)

    print(f"Generated {args.frames} frames | "
          f"Δt ∈ [{args.dt_min}, {args.dt_max}] | "
          f"{next_id} unique objects")

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    main()

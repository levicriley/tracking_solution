#!/usr/bin/env python3
"""
Synthetic tracker-test generator  —  now with variable Δt.

New CLI flags
-------------
  --dt-min  t_min   minimum inter-frame interval (seconds)
  --dt-max  t_max   maximum inter-frame interval (seconds)

All other flags (drop-prob, drift, #objects, lifetimes, …) unchanged.
"""
import json, argparse, random, datetime as dt
from pathlib import Path

def clamp(v, lo=0.0, hi=1.0):        # keep coords inside frame
    return max(lo, min(v, hi))

# --------------------------------------------------------------------------
def main():
    p = argparse.ArgumentParser(        
        description="Synthetic test-data generator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("-f", "--frames", type=int, default=30,
                   help="total #frames in the sequence")

    # variable frame spacing
    p.add_argument("--dt-min", type=float, default=0.030,
                   help="min frame interval (s)")
    p.add_argument("--dt-max", type=float, default=0.040,
                   help="max frame interval (s)")

    # object-count & lifetime bounds
    p.add_argument("--min-objects", type=int, default=1, help="minimum number of objects on screen at once")
    p.add_argument("--max-objects", type=int, default=5, help="maximum number of objects on screen at once")
    p.add_argument("--min-life",    type=int, default=5, help="minimum lifetime of objects")
    p.add_argument("--max-life",    type=int, default=20, help="maximum lifetime of objects")

    # motion + noise
    p.add_argument("--step-max",  type=float, default=0.08, help="max true center position in x and y")
    p.add_argument("--bias-x",    type=float, default=0.0, help="average velocity x")
    p.add_argument("--bias-y",    type=float, default=0.0, help="average velocity y")
    p.add_argument("--noise-pos", type=float, default=0.0001, help="position measurement jitter")
    p.add_argument("--noise-size",type=float, default=0.0001, help="size measurement jitter")

    # drop-outs
    p.add_argument("--drop-prob", type=float, default=0.1, help="chance of dropping out temporarily")
    p.add_argument("--drop-max",  type=int,   default=3, help="maximum number of frames it can drop out")

    p.add_argument("--seed",      type=int,   default=0, help="rng seed")
    args = p.parse_args()

    rng = random.Random(args.seed)
    Path("tests").mkdir(exist_ok=True)

    # ------------ set up objects ----------------------------------------
    objects, next_id = [], 0
    for frame in range(args.frames):
        active = [o for o in objects if o["start"] <= frame < o["end"]]
        need   = rng.randint(args.min_objects, args.max_objects) - len(active)
        for _ in range(max(0, need)):
            life = rng.randint(args.min_life, args.max_life)
            objects.append({"id": next_id,
                            "start": frame,
                            "end":   min(frame + life, args.frames),
                            "x": rng.uniform(0.1, 0.8),
                            "y": rng.uniform(0.1, 0.8),
                            "drop_remaining": 0})
            next_id += 1

    # ------------ time-stamp list with variable Δt ----------------------
    ts_list = [dt.datetime(2025, 3, 24, 18, 0, 0)]
    for _ in range(1, args.frames):
        delta = rng.uniform(args.dt_min, args.dt_max)
        ts_list.append(ts_list[-1] + dt.timedelta(seconds=delta))

    frames_json, expected_json = [], []
    for k, ts in enumerate(ts_list):
        ts_str  = ts.strftime("%Y-%m-%dT%H:%M:%S.%f")
        detections, tracks = [], []

        for obj in objects:
            if not (obj["start"] <= k < obj["end"]):
                continue

            # maybe start / continue a drop-out
            if obj["drop_remaining"] == 0 and rng.random() < args.drop_prob:
                obj["drop_remaining"] = rng.randint(1, args.drop_max)
            if obj["drop_remaining"] > 0:
                obj["drop_remaining"] -= 1
                dropped_this_frame = True
            else:
                dropped_this_frame = False

            # latent random-walk + drift
            obj["x"] = clamp(obj["x"] + rng.uniform(-args.step_max, args.step_max) + args.bias_x)
            obj["y"] = clamp(obj["y"] + rng.uniform(-args.step_max, args.step_max) + args.bias_y)

            # generate detection only if not dropped
            if not dropped_this_frame:
                mx = clamp(obj["x"] + rng.gauss(0, args.noise_pos))
                my = clamp(obj["y"] + rng.gauss(0, args.noise_pos))
                w  = clamp(0.12 + rng.gauss(0, args.noise_size), hi=0.5)
                h  = clamp(0.12 + rng.gauss(0, args.noise_size), hi=0.5)

                det = {"x": mx, "y": my, "w": w, "h": h}
                detections.append(det)
                tracks.append({"id": obj["id"], **det})

        frames_json.append(  {"timestamp": ts_str, "detections": detections})
        expected_json.append({"timestamp": ts_str, "tracks":     tracks})

    with open("tests/input.json", "w")  as f: json.dump(frames_json,  f, indent=2)
    with open("tests/expected.json", "w") as f: json.dump(expected_json, f, indent=2)

    print(f"Frames: {args.frames} | Δt∈[{args.dt_min},{args.dt_max}] "
          f"| {next_id} total objects")

if __name__ == "__main__":
    main()


#!/usr/bin/env python3
"""
Synthetic tracker-test generator
────────────────────────────────
• Variable Δt between frames
• Configurable #objects and lifetimes
• Motion:
      – Random walk per object           (--step-max)
      – Constant drift                   (--bias-x / --bias-y)
      – Shared wind gust                 (--wind-sigma / --wind-max)
      – Independent sway per plant       (--sway-sigma)
      – NEW: Global rotation & zoom      (--rot-sigma / --scale-sigma)
• Box-size bounds                        (--min-size / --max-size)
• Gaussian measurement noise             (--noise-pos / --noise-size)
• Probabilistic drop-outs                (--drop-prob / --drop-max)
• Gap-free IDs on first appearance, persistent thereafter
Loads defaults from an optional defaults.ini ([generator] section);
CLI flags still override.
Outputs: tests/input.json, tests/expected.json
"""

import json, argparse, random, datetime as dt, configparser, os, math
from pathlib import Path
from typing import Dict, Any, List

# ---------------------------------------------------------------------------
def clamp(v: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return max(lo, min(v, hi))

# ---------------------------------------------------------------------------
def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Synthetic tracker-test generator.\n\n"
                    "Legend in help text:\n"
                    "  •   [motion-global]     – real motion applied equally to ALL objects\n"
                    "  •   [motion-object]     – real motion added independently per object\n"
                    "  •   [noise-global]      – measurement-only noise applied to EVERY detection\n"
                    "  •   [noise-object]      – measurement-only noise added per detection box",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    # -------- sequence & timing -----------------------------------------
    p.add_argument("-f", "--frames", type=int, default=30,
        help="total number of frames")
    p.add_argument("--dt-min", type=float, default=0.030,
        help="minimum inter-frame interval (s)")
    p.add_argument("--dt-max", type=float, default=0.040,
        help="maximum inter-frame interval (s)")

    # -------- population -------------------------------------------------
    p.add_argument("--min-objects", type=int, default=1,
        help="minimum visible objects per frame            [motion-object]")
    p.add_argument("--max-objects", type=int, default=5,
        help="maximum visible objects per frame            [motion-object]")
    p.add_argument("--min-life", type=int, default=5,
        help="minimum lifetime of an object (frames)       [motion-object]")
    p.add_argument("--max-life", type=int, default=20,
        help="maximum lifetime of an object (frames)       [motion-object]")

    # -------- size bounds ------------------------------------------------
    p.add_argument("--min-size", type=float, default=0.10,
        help="lower bound for w & h                        [motion-object]")
    p.add_argument("--max-size", type=float, default=0.20,
        help="upper bound for w & h                        [motion-object]")

    # -------- REAL MOTION -----------------------------------------------
    # global components
    p.add_argument("--bias-x", type=float, default=0.0,
        help="constant x-drift (e.g. camera pan)           [motion-global]")
    p.add_argument("--bias-y", type=float, default=0.0,
        help="constant y-drift                             [motion-global]")
    p.add_argument("--wind-sigma", type=float, default=0.02,
        help="σ of shared wind random walk                 [motion-global]")
    p.add_argument("--wind-max", type=float, default=0.10,
        help="cap on |wind| magnitude                      [motion-global]")
    p.add_argument("--rot-sigma", type=float, default=0.00,
        help="σ of in-plane rotation per frame (rad)       [motion-global]")
    p.add_argument("--scale-sigma", type=float, default=0.00,
        help="σ of zoom factor per frame (1±σ)             [motion-global]")

    # per-object components
    p.add_argument("--step-max", type=float, default=0.00,
        help="random-walk radius per object                [motion-object]")
    p.add_argument("--sway-sigma", type=float, default=0.01,
        help="leaf/canopy sway σ per object                [motion-object]")

    # -------- MEASUREMENT NOISE -----------------------------------------
    p.add_argument("--noise-pos", type=float, default=0.002,
        help="centre localisation noise σ                  [noise-object]")
    p.add_argument("--noise-size", type=float, default=0.001,
        help="size   localisation noise σ                  [noise-object]")

    # -------- drop-outs --------------------------------------------------
    p.add_argument("--drop-prob", type=float, default=0.05,
        help="probability a visible object vanishes        [occlusionnoise]")
    p.add_argument("--drop-max", type=int, default=3,
        help="maximum consecutive dropped frames           [occlusionnoise]")

    # -------- misc -------------------------------------------------------
    p.add_argument("--seed", type=int, default=0,
        help="RNG seed for reproducibility")

    return p


# ---------------------------------------------------------------------------
def inject_ini_defaults(parser: argparse.ArgumentParser,
                        ini_path="defaults.ini"):
    if not os.path.exists(ini_path):
        print(f"could not find {ini_path}")
        return
    print(f"found {ini_path}: parsing defaults")
    cfg = configparser.ConfigParser()
    cfg.read(ini_path)
    if "generator" not in cfg: return
    defaults = {k.replace("-", "_"): eval(v) for k, v in cfg["generator"].items()}
    parser.set_defaults(**defaults)

# ---------------------------------------------------------------------------
def main() -> None:
    parser = build_arg_parser()
    inject_ini_defaults(parser)
    args = parser.parse_args()
    rng  = random.Random(args.seed)

    Path("tests").mkdir(exist_ok=True)

    # -------- create object catalogue -----------------------------------
    objects: List[Dict[str, Any]] = []
    next_id = 0
    for frame in range(args.frames):
        active = [o for o in objects if o["start"] <= frame < o["end"]]
        spawn   = rng.randint(args.min_objects, args.max_objects) - len(active)
        for _ in range(max(0, spawn)):
            life  = rng.randint(args.min_life, args.max_life)
            size0 = rng.uniform(args.min_size, args.max_size)
            ax, ay = rng.uniform(0.1, 0.8), rng.uniform(0.1, 0.8)
            objects.append({
                "id": next_id,
                "start": frame, "end": min(frame + life, args.frames),
                "anchor_x": ax, "anchor_y": ay,
                "x": ax, "y": ay, "w": size0, "h": size0,
                "drop_remaining": 0, "seen_once": False,
            })
            next_id += 1

    # -------- build timestamp list --------------------------------------
    ts = dt.datetime(2025, 3, 24, 18, 0, 0)
    ts_list = [ts]
    for _ in range(1, args.frames):
        ts += dt.timedelta(seconds=rng.uniform(args.dt_min, args.dt_max))
        ts_list.append(ts)

    # shared wind vector
    wind_x = wind_y = 0.0

    frames_out, tracks_out = [], []

    # -------- main simulation loop --------------------------------------
    for k, tstamp in enumerate(ts_list):
        # evolve wind (bounded walk)
        wind_x += rng.gauss(0, args.wind_sigma)
        wind_y += rng.gauss(0, args.wind_sigma)
        mag = (wind_x**2 + wind_y**2) ** 0.5
        if mag > args.wind_max:
            wind_x *= args.wind_max / mag
            wind_y *= args.wind_max / mag

        # sample global rot & scale
        theta = rng.gauss(0, args.rot_sigma)
        scale = 1.0 + rng.gauss(0, args.scale_sigma)
        cos_t, sin_t = math.cos(theta), math.sin(theta)

        dets, trks = [], []
        for obj in objects:
            if not (obj["start"] <= k < obj["end"]): continue

            # drop-out bookkeeping
            if obj["drop_remaining"] == 0 and obj["seen_once"]:
                if rng.random() < args.drop_prob:
                    obj["drop_remaining"] = rng.randint(1, args.drop_max)
            dropped = obj["drop_remaining"] > 0
            if dropped: obj["drop_remaining"] -= 1

            # motion: bias + wind + optional per-object walk + sway
            dx_shared = args.bias_x + wind_x
            dy_shared = args.bias_y + wind_y
            dx_obj = rng.uniform(-args.step_max, args.step_max) if args.step_max else 0.0
            dy_obj = rng.uniform(-args.step_max, args.step_max) if args.step_max else 0.0
            dx_sway = rng.gauss(0, args.sway_sigma)
            dy_sway = rng.gauss(0, args.sway_sigma)

            # new anchor-relative position
            cx = obj["anchor_x"] + dx_shared + dx_obj + dx_sway
            cy = obj["anchor_y"] + dy_shared + dy_obj + dy_sway

            # apply global rotation+scale about image centre (0.5,0.5)
            dx, dy = cx - 0.5, cy - 0.5
            cx = 0.5 + scale * (cos_t*dx - sin_t*dy)
            cy = 0.5 + scale * (sin_t*dx + cos_t*dy)

            obj["x"], obj["y"] = clamp(cx), clamp(cy)

            # update size (zoom) within bounds
            size_noise = rng.gauss(0, args.noise_size)
            obj["w"] = clamp(obj["w"] * scale + size_noise,
                             lo=args.min_size, hi=args.max_size)
            obj["h"] = clamp(obj["h"] * scale + size_noise,
                             lo=args.min_size, hi=args.max_size)

            if not dropped:
                mx = clamp(obj["x"] + rng.gauss(0, args.noise_pos))
                my = clamp(obj["y"] + rng.gauss(0, args.noise_pos))
                det = {"x": mx, "y": my, "w": obj["w"], "h": obj["h"]}
                dets.append(det)
                trks.append({"id": obj["id"], **det})
                obj["seen_once"] = True

        frames_out.append({"timestamp": tstamp.strftime("%Y-%m-%dT%H:%M:%S.%f"),
                           "detections": dets})
        tracks_out.append({"timestamp": frames_out[-1]["timestamp"],
                           "tracks": trks})

    # -------- save -------------------------------------------------------
    Path("tests").mkdir(exist_ok=True)
    with open("tests/input.json", "w")   as f: json.dump(frames_out,  f, indent=2)
    with open("tests/expected.json", "w") as f: json.dump(tracks_out, f, indent=2)

    print(f"Frames: {args.frames} | "
          f"Δt ∈ [{args.dt_min}, {args.dt_max}] | "
          f"{next_id} unique objects | "
          f"step-max {args.step_max} | "
          f"rot-sigma {args.rot_sigma} | scale-sigma {args.scale_sigma}")

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    main()

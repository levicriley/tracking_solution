#!/usr/bin/env python3
"""
json2png.py  –  Render frames from a detection / tracking JSON file.

The input schema is the same as your generator and tracker outputs:

[
  {
    "timestamp": "2025-03-24T18:00:00.000000",
    "detections": [
      { "x": 0.12, "y": 0.41, "w": 0.10, "h": 0.10 }
    ]
    # OR "tracks": [ { "id": 7, ... } ]  – either field works
  },
  ...
]

Usage
-----
python json2png.py --input detections.json --out-dir vis \
                   --width 800 --height 600 \
                   --id-field tracks        # or detections

Dependencies
------------
pip install opencv-python
"""

import argparse, json, os, cv2, numpy as np
from pathlib import Path

# ---------------------------------------------------------------------------

def parse_args():
    p = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description="Render bounding boxes from JSON to PNGs")
    p.add_argument("--input",   "-i", required=True,
                   help="JSON file with 'detections' or 'tracks'")
    p.add_argument("--out-dir", "-o", required=True,
                   help="output directory for PNGs")
    p.add_argument("--width",   "-W", type=int, default=800,
                   help="output image width")
    p.add_argument("--height",  "-H", type=int, default=600,
                   help="output image height")
    p.add_argument("--id-field", choices=["tracks", "detections"],
                   default="tracks",
                   help="which list to read from each frame object")
    p.add_argument("--font-scale", type=float, default=0.5,
                   help="OpenCV font scale for IDs")
    return p.parse_args()

# ---------------------------------------------------------------------------

def draw_frame(objs, img_w, img_h, font_scale):
    """
    Paint each bbox, and if the dict contains 'id', print it in the top-left above the box.
    """
    img = np.full((img_h, img_w, 3), 30, dtype=np.uint8)

    for obj in objs:
        x = int(obj["x"] * img_w)
        y = int(obj["y"] * img_h)
        w = int(obj["w"] * img_w)
        h = int(obj["h"] * img_h)

        cv2.rectangle(img, (x, y), (x + w, y + h), (0, 255, 0), 2)

        if "id" in obj:
            text = str(obj["id"])
            (tw, th), _ = cv2.getTextSize(
                text, cv2.FONT_HERSHEY_SIMPLEX, font_scale, 1)
            text_x = x + 2
            text_y = max(y - 4, th + 2)  # prevent going above image top
            cv2.putText(
                img, text, (text_x, text_y),
                cv2.FONT_HERSHEY_SIMPLEX, font_scale,
                (0, 255, 255), 1, cv2.LINE_AA)

    return img




# ---------------------------------------------------------------------------

def main():
    args = parse_args()
    Path(args.out_dir).mkdir(parents=True, exist_ok=True)

    with open(args.input) as f:
        frames = json.load(f)

    for idx, frame in enumerate(frames):
        objs = frame.get(args.id_field, [])
        # fall back to "detections" if we asked for "tracks" but it's absent
        if args.id_field == "tracks" and not objs:
            objs = frame.get("detections", [])
        has_ids = (args.id_field == "tracks")
        img = draw_frame(objs, args.width, args.height, args.font_scale)


        out_name = Path(args.out_dir) / f"frame_{idx:04d}.png"
        cv2.imwrite(out_name.as_posix(), img)

    print(f"Wrote {len(frames)} PNGs to {args.out_dir}")

# ---------------------------------------------------------------------------

if __name__ == "__main__":
    main()
 

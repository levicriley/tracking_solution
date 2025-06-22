#!/usr/bin/env python3
"""
compare_tracks.py – Compare output tracking JSON to expected JSON,
penalizing ID switches across frames even if exact IDs differ.

Usage:
    python compare_tracks.py expected.json output.json
"""

import json
import sys
import numpy as np
from scipy.spatial.distance import euclidean

def load_json(path):
    with open(path) as f:
        return json.load(f)

def obj_key(obj):
    """Return (x, y, w, h) tuple as the unique object key."""
    return (obj["x"], obj["y"], obj["w"], obj["h"])

def find_closest_match(target, candidates, threshold=0.01):
    """Find the closest match from candidates for a target object."""
    tx, ty, tw, th = obj_key(target)
    for idx, cand in enumerate(candidates):
        cx, cy, cw, ch = obj_key(cand)
        dist = euclidean((tx, ty, tw, th), (cx, cy, cw, ch))
        if dist < threshold:
            return idx
    return -1

def compare_tracks(expected, actual, threshold=0.01):
    expected_obj_to_id = {}  # maps object key -> output track ID
    id_switches = 0
    seen_keys = set()

    for frame_idx, (exp_frame, act_frame) in enumerate(zip(expected, actual)):
        exp_objs = exp_frame.get("tracks", [])
        act_objs = act_frame.get("tracks", [])

        for exp_obj in exp_objs:
            key = obj_key(exp_obj)
            match_idx = find_closest_match(exp_obj, act_objs, threshold)
            if match_idx == -1:
                continue  # can't match this expected object

            out_id = act_objs[match_idx]["id"]
            if key in expected_obj_to_id:
                if expected_obj_to_id[key] != out_id:
                    id_switches += 1  # this object was tracked by a different ID
            else:
                expected_obj_to_id[key] = out_id

            seen_keys.add(key)

    return {
        "frames_compared": len(expected),
        "tracked_objects": len(seen_keys),
        "id_switches": id_switches,
    }

def main():

    if len(sys.argv) != 3:
        print("Usage: python compare_tracks.py expected.json output.json")
        sys.exit(1)

    expected = load_json(sys.argv[1])
    actual = load_json(sys.argv[2])

    if len(expected) != len(actual):
        print("Frame count mismatch")
        sys.exit(1)

    results = compare_tracks(expected, actual)
    print("Comparison Results:")
    for k, v in results.items():
        print(f"{k}: {v}")

    if results["id_switches"] == 0:
        print("TEST PASSES ✅")
        sys.exit(0)
    else:
        print("TEST FAILS ❌")
        sys.exit(1)

if __name__ == "__main__":
    main()

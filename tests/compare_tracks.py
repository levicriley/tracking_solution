#!/usr/bin/env python3
"""
compare_tracks.py – Compare output tracking JSON to expected JSON,
penalizing ID switches across frames even if exact IDs differ.

Usage:
    python compare_tracks.py expected.json actual.json
"""

import json
import sys
import numpy as np
from scipy.optimize import linear_sum_assignment

def load_json(path):
    with open(path) as f:
        return json.load(f)

def center(box):
    return (box["x"] + box["w"] / 2, box["y"] + box["h"] / 2)

def iou(a, b):
    ax1, ay1 = a["x"], a["y"]
    ax2, ay2 = ax1 + a["w"], ay1 + a["h"]
    bx1, by1 = b["x"], b["y"]
    bx2, by2 = bx1 + b["w"], by1 + b["h"]

    inter_x1 = max(ax1, bx1)
    inter_y1 = max(ay1, by1)
    inter_x2 = min(ax2, bx2)
    inter_y2 = min(ay2, by2)

    inter_area = max(0, inter_x2 - inter_x1) * max(0, inter_y2 - inter_y1)
    a_area = a["w"] * a["h"]
    b_area = b["w"] * b["h"]
    union_area = a_area + b_area - inter_area
    return inter_area / union_area if union_area > 0 else 0

def match_tracks(gt_tracks, pred_tracks, iou_threshold=0.3):
    cost = np.ones((len(gt_tracks), len(pred_tracks))) * 1e6
    for i, gt in enumerate(gt_tracks):
        for j, pr in enumerate(pred_tracks):
            score = iou(gt, pr)
            if score > iou_threshold:
                cost[i][j] = 1.0 - score
    row_ind, col_ind = linear_sum_assignment(cost)
    matches = []
    for r, c in zip(row_ind, col_ind):
        if cost[r][c] < 1.0:
            matches.append((gt_tracks[r], pred_tracks[c]))
    return matches

def compare_frames(expected, actual):
    id_map = {}  # Maps expected IDs to actual IDs
    id_switches = 0
    misses = 0
    false_positives = 0

    for frame_idx, (e_frame, a_frame) in enumerate(zip(expected, actual)):
        e_tracks = e_frame.get("tracks", [])
        a_tracks = a_frame.get("tracks", [])

        matches = match_tracks(e_tracks, a_tracks)
        used_actual_ids = set()
        for gt, pr in matches:
            eid = gt["id"]
            aid = pr["id"]
            if eid not in id_map:
                id_map[eid] = aid
            elif id_map[eid] != aid:
                id_switches += 1  # switched identity
            used_actual_ids.add(aid)

        misses += len(e_tracks) - len(matches)
        false_positives += len(a_tracks) - len(matches)

    return {
        "frames_compared": len(expected),
        "id_switches": id_switches,
        "misses": misses,
        "false_positives": false_positives,
    }

def main():
    if len(sys.argv) != 3:
        print("Usage: python compare_tracks.py expected.json actual.json")
        sys.exit(1)

    expected = load_json(sys.argv[1])
    actual = load_json(sys.argv[2])
    if len(expected) != len(actual):
        print("Frame count mismatch")
        sys.exit(1)

    results = compare_frames(expected, actual)
    print("Comparison Results:")
    for k, v in results.items():
        print(f"{k}: {v}")

    if results["misses"] == 0 and results["id_switches"] == 0 and results["false_positives"] == 0:
        print("TEST PASSES ✅")
        sys.exit(0)
    else:
        print("TEST FAILS ❌")
        sys.exit(1)

if __name__ == "__main__":
    main()

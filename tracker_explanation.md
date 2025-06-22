
# Explanation of C++ Tracker Code

This document explains the C++ source code implementing the object tracker in `main.cpp` and supporting classes.

---

## Overview

The program performs multi-object tracking by:

1. Loading detections from an input JSON file.
2. Updating object tracks using a Kalman Filter and assignment logic.
3. Saving the resulting tracks to an output JSON file.
4. Drawing bounding boxes with IDs to visualization images.

---

## Main Components

### `main`

```cpp
int main(int argc, char** argv)
```

#### Purpose:
Runs the tracker from the command line. Accepts arguments for input/output paths, parameters, or falls back to reading from `defaults.ini`.

#### Key steps:
- Loads values from `defaults.ini` using `get_ini_value`.
- Uses `CLI11` to allow CLI overrides of config.
- Loads input frames using `load_frames`.
- Iteratively calls `tracker.step()` for each frame.
- Calls `save_tracks` and `draw_vis` to store output.

---

## Supporting Functions

### `parse_iso`, `format_iso`

Converts between ISO timestamps (used in JSON) and `double` seconds-since-epoch (used internally).

---

### `load_frames`

```cpp
static std::vector<Frame> load_frames(const std::string& path)
```

Parses a JSON file containing detections and validates that bounding boxes have non-zero width and height.

---

### `save_tracks`

```cpp
static void save_tracks(const std::string& path, ...)
```

Saves a JSON file of tracked objects per frame, including:
- `timestamp`
- Array of objects with `id`, `x`, `y`, `w`, `h`

---

### `draw_vis`

```cpp
static void draw_vis(const std::string& dir, int idx, ...)
```

Draws rectangles and labels for all tracks on a blank image for each frame, saved as a `.png`.

---

## `Tracker` Class

Defined in `Tracker.hpp` and `Tracker.cpp`. Uses OpenCV's Kalman Filter to track bounding boxes over time.

### Key methods:

#### `step`

```cpp
void Tracker::step(double ts, const std::vector<Detection>& dets)
```

- **Predicts** next positions of all tracks.
- Computes **cost matrix** using distance and IoU.
- Applies **Hungarian algorithm** to match tracks to detections.
- Updates matched tracks.
- Initializes new tracks for unmatched detections.
- Removes stale tracks (not updated for `max_age` frames).

#### `create_kf`, `set_F`, `set_Q`

Helper functions for setting up and updating the Kalman Filter transition and noise matrices.

---

## Data Structures

### `Frame`

```cpp
struct Frame {
    double ts;
    std::vector<Detection> dets;
};
```

Represents a single frame's timestamp and detections.

---

### `Detection` and `Track`

```cpp
struct Detection {
    double x, y, w, h;
};

struct Track {
    int id;
    cv::Mat rect;
    cv::KalmanFilter kf;
    double last_ts;
    int time_since_update = 0;
    int age = 0;
};
```

---

## Parameters

- `max_dist`: Maximum allowed center distance to match.
- `alpha`: Weight between IoU and center distance (0=only IoU, 1=only distance).
- `max_age`: How many frames to keep an unmatched track before discarding.

---

## Dependencies

- [OpenCV](https://opencv.org/)
- [CLI11](https://github.com/CLIUtils/CLI11)
- [nlohmann/json](https://github.com/nlohmann/json)

---

## Example CLI

```sh
./tracking-solution --input data/in.json --output data/out.json --vis-dir vis/
```

If arguments are omitted, falls back to values in `defaults.ini`.

---

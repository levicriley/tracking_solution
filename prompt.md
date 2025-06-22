# Agricultural Robotics Take-Home Challenge: Crop Tracking System

## Overview

You are tasked with implementing a simplified crop tracking system that can maintain persistent object IDs across video frames. This challenge focuses on the core tracking algorithm rather than building an entire distributed system from scratch.

## Background

Agricultural robots need to reliably identify and track crops in the field to perform precision operations. A key challenge is maintaining consistent object IDs even when objects temporarily disappear from view or are partially occluded.

## Requirements

1. **Object Tracking System**:
   - Implement a tracking system that maintains persistent IDs for detected objects
   - The system should handle cases where objects temporarily disappear from view (for 1-3 frames)
   - Objects that reappear in similar positions should retain their original IDs

2. **Simple Visualization**:
   - Create a basic visualization showing tracked objects with their IDs
   - Visualize the tracking history (e.g., previous positions)
   - Save visualizations as image files to a specified output directory (e.g., `/data/visualization/`)
   - At minimum, generate one summary image showing tracking performance

3. **Containerization**:
   - Package your solution in a Docker container
   - Provide a simple way to run and test your solution

## Technical Constraints

- Your solution must run in a Docker container
- We will execute your solution with the following command structure:
  ```
  docker run -v $(pwd):/data tracking-solution --input /data/input_data.json --output /data/tracking_output.json --vis-dir /data/visualization
  ```
- Your solution should:
  - Read the input file (specified by the `--input` parameter)
  - Write the tracking results to the output file (specified by the `--output` parameter)
  - Save visualizations to the directory specified by the `--vis-dir` parameter
- Unit tests are not required (but encouraged), and we will run an evaluator to test your solution against a random input

## Input and Output Formats

### Input Format

The input file is a JSON array where each element represents a frame with detected objects. Each frame has the following structure:

```json
{
  "frame_id": 123,
  "timestamp": "2025-03-24T18:00:00.000000",
  "detections": [
    {
      "x": 0.65,       // center x-coordinate (normalized 0-1)
      "y": 0.42,       // center y-coordinate (normalized 0-1)
      "width": 0.05,   // width (normalized 0-1)
      "height": 0.05   // height (normalized 0-1)
    },
    // more detections...
  ]
}
```

The array contains frames in sequential order, and each frame contains a list of object detections with their positions and sizes.

### Expected Output Format

Your solution should produce a JSON array with the same number of frames as the input. Each frame should contain tracked objects with persistent IDs:

```json
{
  "frame_id": 123,
  "timestamp": "2025-03-24T18:00:00.000000Z",
  "tracked_objects": [
    {
      "id": 5,         // persistent object ID
      "x": 0.65,       // center x-coordinate (normalized 0-1)
      "y": 0.42,       // center y-coordinate (normalized 0-1)
      "width": 0.05,   // width (normalized 0-1)
      "height": 0.05   // height (normalized 0-1)
    },
    // more tracked objects...
  ]
}
```

The key difference is that each object now has a persistent "id" that should remain the same across frames for the same physical object.

## License

Any code you submit must be released under the MIT license. By submitting your code, you agree to release it under the MIT license.
# Docker‑ready Tracking Solution

Usage (from host):

```bash
docker build -t tracking-solution .
docker run --rm -v $(pwd):/data tracking-solution \
    --input   /data/input_data.json \
    --output  /data/tracking_output.json \
    --vis-dir /data/visualization
```

* Reads JSON detections with ISO timestamps.
* Outputs per‑frame tracks with persistent IDs.
* Saves PNG visualisations in `/data/visualization`.

## Run bundled tests
```bash
tests/run_tests.sh
```

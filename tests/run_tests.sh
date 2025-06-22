#!/usr/bin/env bash
set -euo pipefail

########################################################################
# Helper: cfg  <section> <key>
# --------  returns the value of  key=  inside  [section]  from defaults.ini
#          • ignores blank lines and inline comments (# or ;)
#          • stops at the next [section] header
########################################################################
cfg() {
  local section=$1 key=$2
  awk -F'=' -v section="$section" -v key="$key" '
    BEGIN { in=0 }
    $0 ~ "\\["section"\\]" { in=1 ; next }
    /^\[.*\]/             { in=0 }
    in {
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", $1)   # trim key
        if ($1 == key) {
            val=$2
            sub(/[;#].*/, "", val)                   # strip inline comment
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", val)
            print val
            exit
        }
    }' defaults.ini
}

########################################################################
# 1) Re-generate input / expected JSON (generator reads [generator] defaults)
########################################################################
python3 tests/generate_input.py

########################################################################
# 2) Build the Docker image (packages auto-pulled via apt.txt / python.txt)
########################################################################
docker build -t tracking-solution-test .

########################################################################
# 3) Extract tracker CLI defaults from [tracker] section
########################################################################
IN=$(cfg tracker input)
OUT=$(cfg tracker output)
VIS=$(cfg tracker vis-dir)
MAX_DIST=$(cfg tracker max-dist)
MAX_AGE=$(cfg tracker max-age)
ALPHA=$(cfg tracker alpha)

########################################################################
# 4) Run tracker in the container (bind-mount repo root as /data)
########################################################################
docker run --rm \
  -v "$(pwd)":/data \
  tracking-solution-test \
    --input   /data/"$IN" \
    --output  /data/"$OUT" \
    --vis-dir /data/"$VIS" \
    --max-dist "$MAX_DIST" \
    --max-age  "$MAX_AGE" \
    --alpha    "$ALPHA"

########################################################################
# 5) Verify results
########################################################################
diff -u tests/expected.json "$OUT" && echo "TESTS PASS"

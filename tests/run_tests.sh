#!/usr/bin/env bash
set -euo pipefail


########################################################################
# Helper: cfg  <section> <key>
# --------  returns the value of  key=  inside  [section]  from defaults.ini
#          • ignores blank lines and inline comments (# or ;)
#          • stops at the next [section] header
########################################################################
cfg() {
  local section="$1"
  local key="$2"
  local in_section=0

  while IFS= read -r line || [ -n "$line" ]; do
    # Trim leading/trailing whitespace
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"

    # Skip blank lines and comments
    [[ -z "$line" || "$line" == \#* || "$line" == \;* ]] && continue

    if [[ "$line" =~ ^\[(.*)\]$ ]]; then
      in_section=0
      [[ "${BASH_REMATCH[1]}" == "$section" ]] && in_section=1
      continue
    fi

    if [[ "$in_section" -eq 1 && "$line" =~ ^([^=]+)=(.*)$ ]]; then
      local current_key="${BASH_REMATCH[1]}"
      local value="${BASH_REMATCH[2]}"

      # Trim whitespace
      current_key="${current_key#"${current_key%%[![:space:]]*}"}"
      current_key="${current_key%"${current_key##*[![:space:]]}"}"
      value="${value#"${value%%[![:space:]]*}"}"
      value="${value%"${value##*[![:space:]]}"}"

      # Remove inline comments
      value="${value%%\#*}"
      value="${value%%;*}"
      value="${value%"${value##*[![:space:]]}"}"

      if [[ "$current_key" == "$key" ]]; then
        echo "$value"
        return 0
      fi
    fi
  done < defaults.ini

  return 1  # key not found
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

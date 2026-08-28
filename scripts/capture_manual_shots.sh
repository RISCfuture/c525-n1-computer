#!/usr/bin/env bash
# Drives the plugin through each display state in a running sim and captures the
# popup for the manual. Requires the N1 window open at the default position.
set -euo pipefail
cd "$(dirname "$0")/.."
RECT="${SFN1_WINDOW_RECT:-993,495,576,450}"
OUT=docs/images
# Centre of the mode knob in screen points, from "knob" in assets/layout.json.
KNOB=$(awk -F, '{ printf "%d,%d", $1 + $3 * 0.485, $2 + $4 * 0.7714 }' <<<"$RECT")
udp() { python3 scripts/xp_udp.py "$@"; }
shot() { sleep "${2:-1}"; screencapture -x -R"$RECT" "$OUT/$1.png"; echo "captured $1"; }
override() {
  udp set sfn1/test_override_rat_c "$1"
  udp set sfn1/test_override_pa_ft "$2"
  udp set sfn1/test_override_on_ground "$3"
  udp set sfn1/test_override_anti_ice "$4"
  udp set sfn1/test_override_powered "$5"
  udp set sfn1/test_override_air_data_failed "${6:-0}"
}
mode_to() {  # 0 CLB, 1 TOGA, 2 CRU
  local target=$1 current
  for _ in 1 2 3 4; do
    current=$(udp get sfn1/mode | awk '{print int($3+0.5)}')
    [ "$current" -eq "$target" ] && return 0
    if [ "$current" -lt "$target" ]; then udp cmnd sfn1/mode_up; else udp cmnd sfn1/mode_down; fi
    sleep 0.3
  done
}
# The selected-temperature readout blinks at 1.5 Hz. Capture a full cycle and
# keep the lit frame: more amber detail makes it the largest PNG of the set.
brightest_shot() {
  local name=$1 dir i
  dir=$(mktemp -d)
  for i in 1 2 3 4 5 6; do
    screencapture -x -R"$RECT" "$dir/$i.png"
    sleep 0.13
  done
  cp "$(stat -f '%z %N' "$dir"/*.png | sort -rn | head -1 | cut -d' ' -f2-)" "$OUT/$name.png"
  rm -rf "$dir"
  echo "captured $name"
}

trap 'cliclick "du:$KNOB" >/dev/null 2>&1 || true' EXIT

udp set sfn1/test_override_enable 1
override 15 0 1 0 0          # unpowered: dark faceplate
shot display-off 2
override 15 0 1 0 1          # power restored: 888 self-test
shot display-self-test 1
sleep 6                      # self-test completes
mode_to 1
shot display-takeoff-n1 1    # TO/GA on the ground: takeoff target
mode_to 0
shot display-dashes 1        # CLB selected on the ground: invalid mode
mode_to 1
override 0 2000 1 2 1        # all bleed anti-ice on
shot display-anti-ice 2
override -30 25000 0 0 1     # airborne
mode_to 0
shot display-climb 2         # max continuous climb target
mode_to 2
override -45 39000 0 0 1
shot display-cruise 2        # max cruise target
mode_to 1
override 10 3000 0 0 1
shot display-go-around 2     # TO/GA in flight is the go-around target

override 15 0 1 0 1 1        # air data fails while running: energised blank
shot display-fail 2
override 15 0 1 0 1          # air data restored
if command -v cliclick >/dev/null; then
  cliclick "m:$KNOB" "dd:$KNOB" >/dev/null   # press and hold the knob
  shot display-rat 1                         # a held knob reads out RAT
  udp cmnd sfn1/temp_up
  udp cmnd sfn1/temp_up
  udp cmnd sfn1/temp_up
  brightest_shot display-temp-set            # rotating while held dials a temp
  cliclick "du:$KNOB" >/dev/null
else
  echo "warning: cliclick not installed; skipped display-rat and display-temp-set" >&2
fi

override 15 0 1 0 0          # leave the device unpowered, then hand back to the sim
sleep 1
udp set sfn1/test_override_enable 0
echo "done"

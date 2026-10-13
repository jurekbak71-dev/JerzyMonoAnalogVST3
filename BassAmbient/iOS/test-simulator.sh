#!/bin/bash
set -euo pipefail
mkdir -p simulator-qa
RUNTIME=$(xcrun simctl list runtimes -j | python3 -c 'import json,sys; r=[x for x in json.load(sys.stdin)["runtimes"] if x.get("isAvailable") and ".iOS-" in x["identifier"]]; print(sorted(r,key=lambda x:tuple(map(int,x["version"].split("."))))[-1]["identifier"])')
DEVICE=$(xcrun simctl create BassAmbient-iPhone14 com.apple.CoreSimulator.SimDeviceType.iPhone-14 "$RUNTIME")
trap 'xcrun simctl shutdown "$DEVICE" || true' EXIT
xcrun simctl boot "$DEVICE"
xcrun simctl bootstatus "$DEVICE" -b
xcrun simctl install "$DEVICE" "$APP_PATH"
SIMCTL_CHILD_BASS_AMBIENT_SMOKE=1 xcrun simctl launch "$DEVICE" pl.jerzy.bassambient.mobile
CONTAINER=$(xcrun simctl get_app_container "$DEVICE" pl.jerzy.bassambient.mobile data)
for attempt in $(seq 1 60); do
  if test -f "$CONTAINER/Documents/smoke-result.txt"; then break; fi
  sleep 2
done
cp -R "$CONTAINER/Documents/." simulator-qa/
xcrun simctl io "$DEVICE" screenshot simulator-qa/iPhone14-screen.png
cat simulator-qa/smoke-result.txt
python3 - <<'PY'
from pathlib import Path
assert Path('simulator-qa/smoke-result.txt').read_text().strip()=='PASS'
assert Path('simulator-qa/smoke.wav').stat().st_size>10000
assert Path('simulator-qa/smoke.mid').stat().st_size>30
PY
# A second process launch must retain the session that the smoke test saved.
xcrun simctl terminate "$DEVICE" pl.jerzy.bassambient.mobile
xcrun simctl launch "$DEVICE" pl.jerzy.bassambient.mobile
sleep 3
xcrun simctl io "$DEVICE" screenshot simulator-qa/iPhone14-relaunch.png

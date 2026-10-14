#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
command -v adb >/dev/null || { echo "Install Android platform-tools (adb), enable USB debugging and connect your phone." >&2; exit 1; }
args=()
if [[ $# == 1 ]]; then args=(-s "$1"); elif [[ $# != 0 ]]; then echo "Usage: install.sh [DEVICE_SERIAL]" >&2; exit 1; fi
adb "${args[@]}" get-state
adb "${args[@]}" install -r LocalClipboard.apk
adb "${args[@]}" shell am start -n org.localclipboard/.ClipboardActivity
echo "Complete device registration on the phone."

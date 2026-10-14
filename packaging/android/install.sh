#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./language.sh
choose_language
if [[ -n ${LC_ACTION:-} ]]; then choose_action; fi
command -v adb >/dev/null || { echo "Install Android platform-tools (adb), enable USB debugging and connect your phone." >&2; exit 1; }
args=()
if [[ $# == 1 ]]; then args=(-s "$1"); elif [[ $# != 0 ]]; then echo "Usage: install.sh [DEVICE_SERIAL]" >&2; exit 1; fi
adb ${args[@]+"${args[@]}"} get-state
installed=$(adb ${args[@]+"${args[@]}"} shell pm path org.localclipboard)
if [[ $installed == *package:* ]]; then choose_action; fi
if [[ ${LC_ACTION:-install} == uninstall ]]; then
    confirm_device_removal
    adb ${args[@]+"${args[@]}"} uninstall org.localclipboard
    exit 0
fi
adb ${args[@]+"${args[@]}"} install -r LocalClipboard.apk
adb ${args[@]+"${args[@]}"} shell am start -n org.localclipboard/.ClipboardActivity --es localclipboard_language "$LC_LANGUAGE"
localized "Завершите регистрацию устройства на телефоне." "Complete device registration on the phone."

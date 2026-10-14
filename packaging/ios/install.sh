#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$ROOT/language.sh"
choose_language
if [[ -n ${LC_ACTION:-} ]]; then choose_action; fi
[[ $(uname -s) == Darwin ]] || { echo 'Install from a Mac with Xcode and the Qt iOS kit.' >&2; exit 1; }
# Without a selected device Xcode handles installation; removal needs its ID.
if [[ -n ${IOS_DEVICE_ID:-} ]]; then
    apps=$(mktemp)
    trap 'rm -f "$apps"' EXIT
    xcrun devicectl device info apps --device "$IOS_DEVICE_ID" --json-output "$apps" >/dev/null
    installed=$(python3 - "$apps" "${IOS_BUNDLE_ID:-org.localclipboard.ios}" <<'APPS'
import json, sys
apps=json.load(open(sys.argv[1]))['result']['apps']
if not isinstance(apps, list): raise ValueError('Invalid installed-app response')
print('yes' if any(app.get('bundleIdentifier') == sys.argv[2] for app in apps) else 'no')
APPS
)
    if [[ $installed == yes ]]; then choose_action; fi
fi
if [[ ${LC_ACTION:-install} == uninstall ]]; then
    : "${IOS_DEVICE_ID:?Set IOS_DEVICE_ID to the iPhone/iPad device identifier}"
    confirm_device_removal
    xcrun devicectl device uninstall app --device "$IOS_DEVICE_ID" "${IOS_BUNDLE_ID:-org.localclipboard.ios}"
    exit 0
fi
: "${QT_IOS_ROOT:?Set QT_IOS_ROOT to your installed Qt iOS kit (for example ~/Qt/6.8.3/ios)}"
# The archive contains source only: no user's profile, device IDs or signing key.
bash "$ROOT/Source/scripts/build" -t ios -c Debug -n -b "$ROOT/build_ios"
if [[ -n ${IOS_DEVICE_ID:-} && -n ${IOS_TEAM_ID:-} ]]; then
    app="$ROOT/build_ios/Debug-iphoneos/LocalClipboard.app"
    [[ -d "$app" ]] || app="$ROOT/build_ios/Debug/LocalClipboard.app"
    xcrun devicectl device install app --device "$IOS_DEVICE_ID" "$app"
    xcrun devicectl device process launch --device "$IOS_DEVICE_ID" "${IOS_BUNDLE_ID:-org.localclipboard.ios}" -- --language "$LC_LANGUAGE"
else
    echo 'In Xcode: select LocalClipboard → Signing & Capabilities → your Team and a unique Bundle Identifier.'
    localized 'Выберите iPhone/iPad и нажмите Run. В мастере регистрации можно выбрать русский или английский.' 'Select your iPhone/iPad and press Run. Choose Russian or English in the registration wizard.'
    open "$ROOT/build_ios/LocalClipboard.xcodeproj"
fi

#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
[[ $(uname -s) == Darwin ]] || { echo 'Install from a Mac with Xcode and the Qt iOS kit.' >&2; exit 1; }
: "${QT_IOS_ROOT:?Set QT_IOS_ROOT to your installed Qt iOS kit (for example ~/Qt/6.8.3/ios)}"
# The archive contains source only: no user's profile, device IDs or signing key.
bash "$ROOT/Source/scripts/build" -t ios -c Debug -n -b "$ROOT/build_ios"
if [[ -n ${IOS_DEVICE_ID:-} && -n ${IOS_TEAM_ID:-} ]]; then
    app="$ROOT/build_ios/Debug-iphoneos/LocalClipboard.app"
    [[ -d "$app" ]] || app="$ROOT/build_ios/Debug/LocalClipboard.app"
    xcrun devicectl device install app --device "$IOS_DEVICE_ID" "$app"
    xcrun devicectl device process launch --device "$IOS_DEVICE_ID" "${IOS_BUNDLE_ID:-org.localclipboard.ios}"
else
    echo 'In Xcode: select LocalClipboard → Signing & Capabilities → your Team and a unique Bundle Identifier.'
    echo 'Select your connected iPhone/iPad and press Run. Register your device when the app opens.'
    open "$ROOT/build_ios/LocalClipboard.xcodeproj"
fi

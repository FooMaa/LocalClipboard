#!/bin/bash
# Extracted to a private temporary directory by the app after confirmation.
set -euo pipefail
dry_run=false
if [[ ${1:-} == --dry-run ]]; then dry_run=true; shift; fi
[[ $# == 5 ]] || exit 2
app=$1
data=$2
parent=$3
language=$4
mode=$5
[[ $parent =~ ^[0-9]+$ && $parent != 0 && $parent != 1 ]] || exit 2
[[ $language == ru || $language == en ]] || exit 2
[[ $mode == erase || $mode == keep ]] || exit 2
[[ $app == /*.app && -f "$app/Contents/Info.plist" && ! -L $app ]] || exit 2
[[ $(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$app/Contents/Info.plist") == org.localclipboard ]] || exit 2
expected_data="$HOME/Library/Application Support/LocalClipboard/LocalClipboard"
[[ $data == "$expected_data" ]] || exit 2
companion="$(dirname "$app")/Uninstall LocalClipboard.app"
if [[ -e $companion ]]; then
    [[ ! -L $companion && $(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$companion/Contents/Info.plist") == org.localclipboard.uninstaller ]] || exit 2
fi
if $dry_run; then
    printf 'app=%s\nautostart=%s\ndata=%s\nmode=%s\n' "$app" "$HOME/Library/LaunchAgents/org.localclipboard.plist" "$data" "$mode"
    exit 0
fi
trap 'rm -f -- "$0"; rmdir -- "$(dirname "$0")" 2>/dev/null || true' EXIT
for ((attempt=0; attempt<30; attempt++)); do
    kill -0 "$parent" 2>/dev/null || break
    sleep 0.1
done
uid=$(id -u)
launchctl disable "gui/$uid/org.localclipboard" 2>/dev/null || true
launchctl bootout "gui/$uid/org.localclipboard" 2>/dev/null || true
rm -f -- "$HOME/Library/LaunchAgents/org.localclipboard.plist"
# Includes older installed versions without the quit IPC command; current user only.
while read -r pid owner executable; do
    if [[ $owner == "$uid" && $executable == "$app/Contents/MacOS/LocalClipboard" ]]; then
        kill -TERM "$pid" 2>/dev/null || true
    fi
done < <(ps -axo pid=,uid=,comm=)
sleep 0.3
if [[ $language == ru ]]; then
    success='LocalClipboard удалён.'; failure='Не удалось удалить LocalClipboard.'
    kept='Профиль и полученные файлы сохранены.'; erased='Локальный профиль и файлы удалены.'
else
    success='LocalClipboard was removed.'; failure='Could not remove LocalClipboard.'
    kept='Your profile and received files were kept.'; erased='Your local profile and files were erased.'
fi
# Finder uses Trash and requests authorization for root-owned apps. Paths are argv.
if ! /usr/bin/osascript - "$app" "$companion" "$failure" <<'APPLESCRIPT'
on run arguments
    try
        tell application "Finder"
            repeat with itemPath in items 1 thru 2 of arguments
                if exists POSIX file (contents of itemPath) then delete POSIX file (contents of itemPath)
            end repeat
        end tell
    on error reason
        display dialog (item 3 of arguments) & return & reason with title "LocalClipboard" buttons {"OK"} default button 1
        error reason
    end try
end run
APPLESCRIPT
then exit 1; fi
message="$success $kept"
if [[ $mode == erase ]]; then
    if [[ -L $data ]]; then rm -f -- "$data"; else rm -rf -- "$data"; fi
    message="$success $erased"
fi
/usr/bin/osascript - "$message" <<'APPLESCRIPT'
on run arguments
    display dialog (item 1 of arguments) with title "LocalClipboard" buttons {"OK"} default button 1
end run
APPLESCRIPT

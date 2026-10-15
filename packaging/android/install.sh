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
# pm path can return 1 when the package has not been installed yet.
installed=$(adb ${args[@]+"${args[@]}"} shell pm path org.localclipboard) || {
    if [[ -n $installed ]]; then echo "$installed" >&2; exit 1; fi
}
if [[ $installed == *package:* ]]; then choose_action; fi
if [[ ${LC_ACTION:-install} == uninstall ]]; then
    confirm_device_removal
    adb ${args[@]+"${args[@]}"} uninstall org.localclipboard
    exit 0
fi
if output=$(adb ${args[@]+"${args[@]}"} install -r LocalClipboard.apk 2>&1); then
    echo "$output"
else
    echo "$output" >&2
    if [[ $output == *INSTALL_FAILED_UPDATE_INCOMPATIBLE* || $output == *INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES* ]]; then
        localized "Подпись APK отличается от установленной версии. Регистрация сохранена. Установите APK с прежним ключом подписи. Если ключ потерян: сначала сохраните код подключения из приложения, затем выберите удаление в установщике и установите APK заново. При удалении данные телефона стираются." "The APK signing key differs from the installed version. Your registration is preserved. Use an APK signed with the original key. If that key is lost: save your connection code from the app, select removal in this installer, then install again. Removal erases the phone's app data." >&2
    elif [[ $output == *INSTALL_FAILED_VERSION_DOWNGRADE* ]]; then
        localized "Установлена более новая версия. Используйте новый APK; регистрация сохранена." "A newer version is installed. Use a newer APK; your registration is preserved." >&2
    elif [[ $output == *INSTALL_FAILED_USER_RESTRICTED* ]]; then
        localized "Установка запрещена настройками устройства или профиля. На Samsung проверьте «Автоблокировку» и разрешение установки из выбранного источника; также возможны ограничения администратора или родительского контроля. Регистрация сохранена." "Installation is blocked by device/profile settings. On Samsung, check Auto Blocker and permission to install from your chosen source; administrator or parental restrictions may also apply. Registration is preserved." >&2
    elif [[ $output == *INSTALL_FAILED_INSUFFICIENT_STORAGE* ]]; then
        localized "Недостаточно места на телефоне. Освободите место и повторите установку; регистрация сохранена." "Not enough space on the phone. Free some storage and retry; registration is preserved." >&2
    elif [[ $output == *INSTALL_FAILED_NO_MATCHING_ABIS* ]]; then
        localized "Архитектура APK не подходит телефону. Для Galaxy S25 используйте APK arm64-v8a. Регистрация сохранена." "The APK architecture does not match the phone. Use the arm64-v8a APK for Galaxy S25. Registration is preserved." >&2
    fi
    exit 1
fi
adb ${args[@]+"${args[@]}"} shell am start -n org.localclipboard/.ClipboardActivity --es localclipboard_language "$LC_LANGUAGE"
localized "Завершите регистрацию устройства на телефоне." "Complete device registration on the phone."

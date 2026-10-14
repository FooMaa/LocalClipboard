#!/usr/bin/env bash
set -euo pipefail
[[ $EUID != 0 ]] || { echo "Run this installer as your desktop user; it will request sudo for package installation." >&2; exit 1; }
command -v apt-get >/dev/null || { echo "This package supports Debian/Ubuntu. Build from source on other distributions." >&2; exit 1; }
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./language.sh
choose_language
if [[ -n ${LC_ACTION:-} ]]; then choose_action; fi
if dpkg-query -W -f='${Status}' localclipboard 2>/dev/null | package_is_installed; then
    choose_action
fi
if [[ ${LC_ACTION:-install} == uninstall ]]; then
        localclipboard --quit || true
        systemctl --user disable --now localclipboard.service 2>/dev/null || true
        rm -f -- "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/localclipboard.desktop" "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/localclipboard.service"
        systemctl --user daemon-reload 2>/dev/null || true
        sudo apt-get remove -y localclipboard
        localized "Удалено. Ваш профиль и файлы сохранены." "Removed. Your profile and files were kept."
        exit 0
fi
sudo apt-get install --reinstall -y ./localclipboard.deb
if [[ -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
    nohup /usr/bin/localclipboard --language "$LC_LANGUAGE" >/dev/null 2>&1 &
else
    localized "Установлено. Откройте LocalClipboard и выберите язык в мастере регистрации." "Installed. Open LocalClipboard to choose a language and register this device."
fi

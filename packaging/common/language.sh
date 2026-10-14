# Sourced by command-line installers. LC_LANGUAGE also supports unattended use.
choose_language() {
    if [[ -z ${LC_LANGUAGE:-} && -t 0 ]]; then
        printf 'Language / Язык: 1 — Русский, 2 — English [2]: '
        read -r choice
        case "$choice" in 1|ru|RU) LC_LANGUAGE=ru ;; *) LC_LANGUAGE=en ;; esac
    fi
    if [[ -z ${LC_LANGUAGE:-} ]]; then
        case ${LANG:-en} in ru*) LC_LANGUAGE=ru ;; *) LC_LANGUAGE=en ;; esac
    fi
    [[ $LC_LANGUAGE == ru || $LC_LANGUAGE == en ]] || { echo 'LC_LANGUAGE must be ru or en.' >&2; return 1; }
}
localized() { if [[ $LC_LANGUAGE == ru ]]; then printf '%s\n' "$1"; else printf '%s\n' "$2"; fi; }

# Destructive actions must be selected explicitly, including in unattended runs.
choose_action() {
    if [[ -z ${LC_ACTION:-} ]]; then
        localized 'Приложение уже установлено: 1 — переустановить, 2 — удалить, 3 — отмена [1]:' 'Application is installed: 1 — reinstall, 2 — uninstall, 3 — cancel [1]:'
        read -r action || action=1
        case "$action" in 1|'') LC_ACTION=reinstall ;; 2) LC_ACTION=uninstall ;; *) exit 0 ;; esac
    fi
    case "$LC_ACTION" in install|reinstall|uninstall) ;; cancel) exit 0 ;; *) echo 'LC_ACTION must be install, reinstall, uninstall or cancel.' >&2; exit 1 ;; esac
}
confirm_device_removal() {
    localized 'Будут удалены приложение и его данные на выбранном устройстве. Введите DELETE для подтверждения:' 'The app and its data will be erased on the selected device. Type DELETE to confirm:'
    read -r confirmation || confirmation=''
    [[ $confirmation == DELETE ]] || exit 0
}
package_is_installed() { local status; read -r status || true; [[ $status == 'install ok installed' ]]; }

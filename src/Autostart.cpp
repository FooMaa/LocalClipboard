#include "Autostart.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#ifdef Q_OS_MACOS
#include <unistd.h>
#endif
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif

namespace {
[[maybe_unused]] bool write(const QString &path, const QString &text, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { if (error) *error = "Cannot create autostart directory."; return false; }
    QSaveFile file(path);
    const QByteArray bytes = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString(); return false;
    }
    return true;
}
[[maybe_unused]] QString quote(QString s) {
    s.replace('\\', "\\\\"); s.replace('"', "\\\""); s.replace('`', "\\`"); s.replace('$', "\\$");
    return '"' + s + '"';
}
[[maybe_unused]] bool remove(const QString &path, QString *error) {
    if (!QFile::exists(path) || QFile::remove(path)) return true;
    if (error) *error = "Cannot remove " + path; return false;
}
}
bool Autostart::setEnabled(bool enabled, QString *error) {
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
#if defined(Q_OS_ANDROID)
    Q_UNUSED(exe); Q_UNUSED(error);
    return AndroidBridge::setEnabled(enabled);
#elif defined(Q_OS_WIN)
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    if (enabled) run.setValue("LocalClipboard", '"' + exe + "\" --background");
    else run.remove("LocalClipboard");
    run.sync();
    if (run.status() != QSettings::NoError) { if (error) *error = "Cannot update Windows login startup."; return false; }
    return true;
#elif defined(Q_OS_MACOS)
    const QString path = QDir::homePath() + "/Library/LaunchAgents/org.localclipboard.plist";
    const QString label = QStringLiteral("gui/%1/org.localclipboard").arg(getuid());
    if (!enabled) {
        // Unloading our own launchd job would terminate the app before saving this preference.
        QProcess::execute("launchctl", {"disable", label});
        return remove(path, error);
    }
    const QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict><key>Label</key><string>org.localclipboard</string>"
        "<key>ProgramArguments</key><array><string>%1</string><string>--background</string></array>"
        "<key>RunAtLoad</key><true/><key>KeepAlive</key><dict><key>SuccessfulExit</key><false/></dict>"
        "<key>LimitLoadToSessionType</key><string>Aqua</string><key>ThrottleInterval</key><integer>5</integer>"
        "</dict></plist>\n").arg(exe.toHtmlEscaped());
    if (!write(path, xml, error)) return false;
    QProcess::execute("launchctl", {"enable", label});
    return true;
#else
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString desktop = config + "/autostart/localclipboard.desktop", service = config + "/systemd/user/localclipboard.service";
    if (!enabled) {
        QProcess::execute("systemctl", {"--user", "disable", "localclipboard.service"});
        return remove(desktop, error) && remove(service, error);
    }
    QString desktopExe = exe; desktopExe.replace('%', "%%");
    const QString entry = QStringLiteral("[Desktop Entry]\nType=Application\nName=LocalClipboard\n"
        "Comment=Synchronize your personal clipboard\nExec=%1 --session-start\nTerminal=false\n"
        "X-GNOME-Autostart-enabled=true\n").arg(quote(desktopExe));
    QString serviceExe = exe; serviceExe.replace('%', "%%");
    const QString unit = QStringLiteral("[Unit]\nDescription=LocalClipboard personal clipboard\n"
        "PartOf=graphical-session.target\nAfter=graphical-session.target\n[Service]\nType=simple\n"
        "ExecStart=%1 --background\nRestart=on-failure\nRestartSec=5\n[Install]\nWantedBy=graphical-session.target\n").arg(quote(serviceExe));
    if (!write(desktop, entry, error) || !write(service, unit, error)) return false;
    // XDG autostart also supports desktops without a systemd user manager.
    QProcess::execute("systemctl", {"--user", "daemon-reload"});
    return true;
#endif
}
bool Autostart::startSessionService() {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QProcess::execute("systemctl", {"--user", "import-environment", "DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"});
    return QProcess::execute("systemctl", {"--user", "start", "localclipboard.service"}) == 0;
#else
    return false;
#endif
}

#include "Autostart.h"
#include "ClipboardSync.h"
#include "Profile.h"
#include "Registration.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif

namespace {
void identity() {
    QCoreApplication::setApplicationName("LocalClipboard");
    QCoreApplication::setOrganizationName("LocalClipboard");
    QCoreApplication::setApplicationVersion("2.0.0");
}
}
int main(int argc, char *argv[]) {
#ifdef Q_OS_ANDROID
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--service") {
            QCoreApplication service(argc, argv); identity();
            return AndroidBridge::runService(service);
        }
    }
#endif
    QApplication app(argc, argv); identity();
#ifdef Q_OS_ANDROID
    QApplication::setQuitOnLastWindowClosed(true); // The receiver lives in its own service process.
#else
    QApplication::setQuitOnLastWindowClosed(false);
#endif
#ifndef Q_OS_ANDROID
    if (app.arguments().contains("--session-start") && Autostart::startSessionService()) return 0;
    QDir().mkpath(Profile::dataDirectory());
    QLockFile lock(QDir(Profile::dataDirectory()).filePath("instance.lock"));
    lock.setStaleLockTime(0);
    const QString ipcName = "localclipboard-" + QString::fromLatin1(QCryptographicHash::hash(
        Profile::dataDirectory().toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    if (!lock.tryLock()) {
        if (!app.arguments().contains("--background")) {
            QLocalSocket socket; socket.connectToServer(ipcName);
            if (socket.waitForConnected(1000)) { socket.write("show"); socket.waitForBytesWritten(1000); }
        }
        return 0;
    }
    QLocalServer::removeServer(ipcName);
    QLocalServer ipc;
    ipc.setSocketOptions(QLocalServer::UserAccessOption);
    ipc.listen(ipcName);
#endif
    Profile profile = Profile::load();
    if (!profile.registered()) {
        if (app.arguments().contains("--background") || app.arguments().contains("--session-start")) return 0;
        if (!registerDevice(&profile)) return 0;
    }
    QString error;
    if (!Autostart::setEnabled(profile.autostart, &error) && !app.arguments().contains("--background"))
        QMessageBox::warning(nullptr, "Autostart", error);
    ClipboardSync window(profile);
#ifndef Q_OS_ANDROID
    QObject::connect(&ipc, &QLocalServer::newConnection, &window, [&] {
        while (ipc.hasPendingConnections()) {
            auto *socket = ipc.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, &window, [&, socket] {
                if (socket->readAll().startsWith("show")) window.showSettings();
                socket->disconnectFromServer();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            if (socket->bytesAvailable()) { window.showSettings(); socket->disconnectFromServer(); }
        }
    });
#endif
    if (!app.arguments().contains("--background") && !app.arguments().contains("--session-start")) window.show();
    return app.exec();
}

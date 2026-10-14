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
#ifdef Q_OS_IOS
#include "IOSBridge.h"
#include <QFileOpenEvent>
#include <functional>
class IOSApplication final : public QApplication {
public:
    using QApplication::QApplication;
    QList<QUrl> pendingFiles;
    QString importError;
    std::function<void(const QList<QUrl> &)> onFiles;
    std::function<void(const QString &)> onImportError;
    bool event(QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            const auto *file = static_cast<QFileOpenEvent *>(event);
            // Copy immediately, including during first-run registration: access to
            // the sending app's security-scoped URL may expire after this event.
            IOSBridge::importFiles({file->url()}, [this](const QList<QUrl> &urls, const QString &error) {
                if (!error.isEmpty()) {
                    if (onImportError) onImportError(error); else importError = error;
                    return;
                }
                if (onFiles) onFiles(urls); else pendingFiles.append(urls);
            });
            return true;
        }
        return QApplication::event(event);
    }
};
#endif
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
#ifdef Q_OS_IOS
    IOSApplication app(argc, argv); identity();
#else
    QApplication app(argc, argv); identity();
#endif
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QApplication::setQuitOnLastWindowClosed(true);
#else
    QApplication::setQuitOnLastWindowClosed(false);
#endif
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
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
#ifdef Q_OS_IOS
    profile.autostart = false;
#endif
    if (!profile.registered()) {
        if (app.arguments().contains("--background") || app.arguments().contains("--session-start")) return 0;
        if (!registerDevice(&profile)) return 0;
    }
    QString error;
    if (!Autostart::setEnabled(profile.autostart, &error) && !app.arguments().contains("--background"))
        QMessageBox::warning(nullptr, "Autostart", error);
    ClipboardSync window(profile);
#ifdef Q_OS_IOS
    app.onFiles = [&window](const QList<QUrl> &urls) { window.importFiles(urls); };
    app.onImportError = [&window](const QString &error) { QMessageBox::warning(&window, "Import", error); };
    if (!app.pendingFiles.isEmpty()) { window.importFiles(app.pendingFiles); app.pendingFiles.clear(); }
    if (!app.importError.isEmpty()) QMessageBox::warning(&window, "Import", app.importError);
#endif
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
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

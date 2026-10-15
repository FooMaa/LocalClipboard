#include "Autostart.h"
#include "ClipboardSync.h"
#include "Profile.h"
#include "Registration.h"
#include "I18n.h"
#include "Ui.h"
#include "Uninstall.h"
#include <QApplication>
#include <QDir>
#include <QIcon>
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
#include "MobileApp.h"
#endif

namespace {
void identity() {
    QCoreApplication::setApplicationName("LocalClipboard");
    QCoreApplication::setOrganizationName("LocalClipboard");
    QCoreApplication::setApplicationVersion(LOCALCLIPBOARD_VERSION);
}
}
int main(int argc, char *argv[]) {
#ifdef Q_OS_ANDROID
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--service") {
            QCoreApplication service(argc, argv); identity(); I18n::initialize();
            return AndroidBridge::runService(service);
        }
    }
#endif
#ifdef Q_OS_IOS
    IOSApplication app(argc, argv); identity();
#else
    QApplication app(argc, argv); identity();
#endif
    const int languageIndex = app.arguments().indexOf("--language");
    QString requestedLanguage = languageIndex >= 0 ? app.arguments().value(languageIndex + 1) : QString();
#ifdef Q_OS_ANDROID
    if (requestedLanguage.isEmpty()) requestedLanguage = AndroidBridge::installationLanguage();
#endif
    I18n::initialize(requestedLanguage);
    Ui::initialize();
    QIcon appIcon;
    for (int size : {32, 64, 128, 256})
        appIcon.addFile(QString(":/icons/localclipboard-%1.png").arg(size));
    QApplication::setWindowIcon(appIcon);
#ifdef Q_OS_ANDROID
    AndroidBridge::setLanguage(I18n::language());
    QObject::connect(&I18n::instance(), &I18n::languageChanged, &app, [] { AndroidBridge::setLanguage(I18n::language()); });
#endif
// Android uses the GPU-rendered Quick interface; Widgets remain the desktop/iOS view.
#ifdef Q_OS_ANDROID
    return runMobileApp(app);
#endif
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (app.arguments().contains("--quit")) return Uninstall::requestQuit() ? 0 : 1;
    if (app.arguments().contains("--uninstall")) return Uninstall::run();
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
    const QString ipcName = Uninstall::ipcName();
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
        QMessageBox::warning(nullptr, I18n::text("Autostart"), I18n::text(error));
    ClipboardSync window(profile);
#ifdef Q_OS_IOS
    app.onFiles = [&window](const QList<QUrl> &urls) { window.importFiles(urls); };
    app.onImportError = [&window](const QString &error) { QMessageBox::warning(&window, I18n::text("Import"), I18n::text(error)); };
    if (!app.pendingFiles.isEmpty()) { window.importFiles(app.pendingFiles); app.pendingFiles.clear(); }
    if (!app.importError.isEmpty()) QMessageBox::warning(&window, I18n::text("Import"), I18n::text(app.importError));
#endif
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    QObject::connect(&ipc, &QLocalServer::newConnection, &window, [&] {
        while (ipc.hasPendingConnections()) {
            auto *socket = ipc.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, &window, [&, socket] {
                // QLocalSocket is a stream: a command may arrive in fragments.
                if (socket->bytesAvailable() < 4) return;
                const QByteArray command = socket->read(4);
                if (command == "quit") app.quit();
                else if (command == "show") { I18n::initialize(); window.showSettings(); }
                socket->disconnectFromServer();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            if (socket->bytesAvailable()) emit socket->readyRead();
        }
    });
#endif
    if (!app.arguments().contains("--background") && !app.arguments().contains("--session-start")) Ui::showWindow(&window);
    return app.exec();
}

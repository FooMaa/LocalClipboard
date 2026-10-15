#include "Async.h"
#include "I18n.h"
#include "Profile.h"
#include "QrCode.h"
#include "Ui.h"
#include "Uninstall.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QScopeGuard>
#include <QSemaphore>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <QTransform>
#include <QUuid>
#include <memory>

class AppTests final : public QObject {
    Q_OBJECT
    QTemporaryDir preferences;
private slots:
    void initTestCase() {
        QVERIFY(preferences.isValid());
        I18n::initialize("en", preferences.path()); Ui::initialize();
    }
    void asynchronousWorkKeepsEventLoopResponsiveAndReturnsToOwner() {
        auto gate = std::make_shared<QSemaphore>();
        bool returned = false;
        QThread *worker = nullptr;
        QThread *delivery = nullptr;
        Async::run(this, [gate] {
            gate->acquire();
            return QPair<int, QThread *>(42, QThread::currentThread());
        }, [&](const QPair<int, QThread *> &result) {
            returned = result.first == 42; worker = result.second; delivery = QThread::currentThread();
        });
        QTimer::singleShot(0, this, [gate] { gate->release(); });
        QTRY_VERIFY(returned);
        QCOMPARE(delivery, thread()); QVERIFY(worker != delivery);

        auto *owner = new QObject;
        auto blocked = std::make_shared<QSemaphore>();
        auto finished = std::make_shared<QSemaphore>();
        bool deliveredAfterDestruction = false;
        Async::run(owner, [blocked, finished] {
            blocked->acquire(); finished->release(); return 1;
        }, [&](int) { deliveredAfterDestruction = true; });
        delete owner;
        blocked->release();
        QVERIFY(finished->tryAcquire(1, 2000));
        QTest::qWait(20);
        QVERIFY(!deliveredAfterDestruction);
    }
    void connectionQrRoundTripAndInvalidInput() {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString previousDirectory = QDir::currentPath();
        const auto restoreDirectory = qScopeGuard([previousDirectory] { QDir::setCurrent(previousDirectory); });
        QVERIFY(QDir::setCurrent(scratch.path()));
        Profile profile; profile.create();
        const QString invitation = profile.invitation();
        const QImage qr = QrCode::encodeInvitation(invitation); QVERIFY(!qr.isNull());
        QCOMPARE(QrCode::decodeInvitation(qr), invitation);
        QCOMPARE(QrCode::decodeInvitation(qr.transformed(QTransform().rotate(90))), invitation);
        QImage inverted = qr; inverted.invertPixels();
        QCOMPARE(QrCode::decodeInvitation(inverted), invitation);
        QCOMPARE(QrCode::decodeInvitation(qr.scaled(256, 256)), invitation);
        QVERIFY(QrCode::encodeInvitation("https://example.com").isNull());
        QVERIFY(QrCode::encodeInvitation("LC2.short").isNull());
        QImage blank(320, 320, QImage::Format_RGB32); blank.fill(Qt::white);
        QVERIFY(QrCode::decodeInvitation(blank).isEmpty());
        Profile joined; QVERIFY(joined.join(QrCode::decodeInvitation(qr)));
        QCOMPARE(joined.groupId(), profile.groupId());
        // Camera/QR decoding must not persist diagnostic image data, including in Debug.
        QVERIFY(QDir(scratch.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }
    void quitCommandTargetsThisUsersInstance() {
        QLocalServer::removeServer(Uninstall::ipcName());
        QLocalServer server; server.setSocketOptions(QLocalServer::UserAccessOption);
        QVERIFY2(server.listen(Uninstall::ipcName()), qPrintable(server.errorString()));
        // Windows named pipes have no write buffer. A blocking client in this
        // thread prevents the server from accepting/reading until the write times out.
        // Production sends this command from another process; exercise that here too.
        QProcess sender;
        sender.start(QCoreApplication::applicationFilePath(),
                     {"--test-request-quit", QCoreApplication::applicationName()});
        QVERIFY2(sender.waitForStarted(), qPrintable(sender.errorString()));
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QLocalSocket> socket(server.nextPendingConnection());
        QTRY_VERIFY(socket->bytesAvailable() >= 4); QCOMPARE(socket->readAll(), QByteArray("quit"));
        QTRY_COMPARE(sender.state(), QProcess::NotRunning);
        QCOMPARE(sender.exitStatus(), QProcess::NormalExit);
        QVERIFY2(sender.exitCode() == 0, sender.readAllStandardError().constData());
    }
    void uninstallKeepsDataUnlessSelected() {
#ifdef Q_OS_MACOS
        bool preservesData = false, hasHelper = QFile::exists(":/uninstall/uninstall.sh");
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(qApp->activeModalWidget());
            if (!dialog) return;
            auto *erase = dialog->findChild<QCheckBox *>();
            preservesData = erase && !erase->isChecked();
            dialog->reject();
        });
        QCOMPARE(Uninstall::run(), 1); QVERIFY(preservesData); QVERIFY(hasHelper);
#endif
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("LocalClipboardTests");
    const int helper = app.arguments().indexOf("--test-request-quit");
    if (helper >= 0) {
        const QString instance = app.arguments().value(helper + 1);
        if (instance.isEmpty()) return 2;
        QCoreApplication::setApplicationName(instance);
        return Uninstall::requestQuit() ? 0 : 1;
    }
    QCoreApplication::setApplicationName("app-tests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    AppTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "AppTests.moc"

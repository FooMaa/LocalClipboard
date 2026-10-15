#include "AppUi.h"
#include "AppController.h"
#include "I18n.h"
#include "AppWindow.h"
#include "ConnectionInfo.h"
#include "QrCode.h"
#include "SyncEngine.h"
#include <QClipboard>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>

class MobileTests final : public QObject {
    Q_OBJECT
    QTemporaryDir preferences;
    static Profile profile() {
        Profile p; p.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.ownerName = QString::fromUtf8("Антон"); p.deviceName = "Galaxy S25"; p.autostart = false; p.create(); return p;
    }
private slots:
    void initTestCase() { I18n::initialize("en", preferences.path()); }
    void screensFitAndTranslate_data() {
        QTest::addColumn<int>("width"); QTest::addColumn<QString>("language"); QTest::addColumn<QString>("platform");
        for (const auto &platform : {QStringLiteral("android"), QStringLiteral("ios"), QStringLiteral("osx")})
            for (int width : {320, 412, 1000})
                for (const auto &language : {QStringLiteral("ru"), QStringLiteral("en")})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(platform).arg(width).arg(language))) << width << language << platform;
    }
    void screensFitAndTranslate() {
        QFETCH(int, width); QFETCH(QString, language); QFETCH(QString, platform);
        I18n::setLanguage(language);
        AppController controller(profile()); QQmlApplicationEngine engine;
        configureAppEngine(engine, controller);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.load(QUrl("qrc:/app/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->setProperty("platformName", platform); window->setMinimumSize(QSize(0, 0));
        window->resize(width, 740); window->show();
        for (const auto &page : {"home", "settings", "register", "pair", "files"}) {
            window->setProperty("page", page); QTest::qWait(40);
            const auto items = window->findChildren<QQuickItem *>();
            for (auto *item : items) {
                if (!item->isVisible() || item->width() == 0) continue;
                const auto wrap = item->property("wrapMode");
                if (wrap.isValid() && item->property("text").isValid()) {
                    const auto text = item->property("text").toString();
                    QVERIFY2(item->width() <= width, qPrintable(text));
                    if (!text.isEmpty()) {
                        const auto position = item->mapToItem(window->contentItem(), QPointF(0, 0));
                        QVERIFY2(position.x() >= -1 && position.x() + item->width() <= width + 1, qPrintable(text));
                    }
                    const auto format = item->property("textFormat");
                    // Untrusted device names and filenames must always be rendered as plain text.
                    if (format.isValid() && item->property("text").toString().contains(controller.deviceName()))
                        QCOMPARE(format.toInt(), 0);
                }
            }
            if ((width == 412 || (width == 1000 && platform == "osx")) && language == "ru" && QString(page) != "pair") {
                const auto image = window->grabWindow();
                QVERIFY(!image.isNull());
                image.save(QDir::current().filePath(QString("interface-%1-%2-%3-ru.png").arg(platform, page).arg(width)));
            }
        }
        for (const auto &warning : warnings) {
            const auto errors = qvariant_cast<QList<QQmlError>>(warning.first());
            for (const auto &error : errors) qWarning().noquote() << error.toString();
        }
        QCOMPARE(warnings.size(), 0);
    }
    void qrRegistrationImportsTheKeyAndAddressesTogether() {
        const auto source = profile();
        const ConnectionInfo invitation{source.invitation(), {"192.168.1.25", "10.0.0.8"}};
        const auto qr = QrCode::encodeInvitation(invitation.encoded()); QVERIFY(!qr.isNull());
        const QString scanned = QrCode::decodeInvitation(qr); QCOMPARE(scanned, invitation.encoded());
        Profile empty = profile(); empty.key.clear(); empty.autostart = false;
        AppController controller(empty);
        const auto fields = controller.parseConnection(scanned);
        QCOMPARE(fields.value("code").toString(), source.invitation());
        QCOMPARE(fields.value("addresses").toString(), invitation.addresses.join(' '));
        QVERIFY(controller.registerDevice("test owner", "test phone", true, scanned));
        QCOMPARE(controller.invitation(), source.invitation());
        QCOMPARE(controller.addresses(), invitation.addresses.join(' '));
        QCOMPARE(Profile::load().peerAddresses, invitation.addresses);
    }
    void languageIsPersistedWithoutResettingRegistration() {
        I18n::setLanguage("en");
        AppWindow application(profile());
        auto *window = application.window(); QVERIFY(window);
        window->setProperty("page", "register");
        auto *owner = window->findChild<QQuickItem *>("ownerInput"); QVERIFY(owner);
        auto *code = window->findChild<QQuickItem *>("codeInput"); QVERIFY(code);
        auto *mode = window->findChild<QQuickItem *>("profileMode"); QVERIFY(mode);
        owner->setProperty("text", QString::fromUtf8("Антон"));
        code->setProperty("text", "LC2.short"); mode->setProperty("join", true);
        application.controller()->setLanguage("ru");
        I18n::initialize({}, preferences.path());
        QCOMPARE(I18n::language(), QStringLiteral("ru"));
        QCOMPARE(owner->property("text").toString(), QString::fromUtf8("Антон"));
        QCOMPARE(code->property("text").toString(), QStringLiteral("LC2.short"));
        QVERIFY(mode->property("join").toBool());
        auto *action = window->findChild<QQuickItem *>("registerAction"); QVERIFY(action);
        QCOMPARE(action->property("text").toString(), I18n::text("Connect this device"));
        QVERIFY(QMetaObject::invokeMethod(action, "clicked"));
        QCOMPARE(window->property("page").toString(), QStringLiteral("register"));
        QVERIFY(!application.controller()->message().isEmpty());
    }
    void unchangedPeersDoNotRefreshTheInterface() {
        AppWindow application(profile());
        auto *engine = application.findChild<SyncEngine *>(); QVERIFY(engine);
        QSignalSpy changed(application.controller(), &AppController::stateChanged);
        engine->peersChanged(); engine->peersChanged();
        QCOMPARE(changed.count(), 0);
    }
    void desktopWindowCanStartHiddenAndKeepsReceivingAfterClose() {
        AppWindow application(profile(), true);
        auto *window = application.window(); QVERIFY(window); QVERIFY(!window->isVisible());
        auto *engine = application.findChild<SyncEngine *>(); QVERIFY(engine); QVERIFY(engine->port());
        application.showSettings(); QTRY_VERIFY(window->isVisible());
        window->setProperty("page", "settings"); window->close();
        QVERIFY(!window->isVisible()); QVERIFY(engine->port());
        const QJsonObject payload{{"kind", "text"}, {"text", "background Quick clipboard"}, {"html", ""}};
        engine->received(payload);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("background Quick clipboard"));
        QApplication::clipboard()->clear();
        engine->received(payload);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("background Quick clipboard"));
        application.showSettings(); QVERIFY(window->isVisible());
        window->showMinimized(); application.showSettings(); QVERIFY(window->visibility() != QWindow::Minimized);
    }
    void switchesWorkWithPointerAndKeyboard_data() {
        QTest::addColumn<QString>("platform");
        for (const auto &platform : {"android", "ios", "osx", "windows", "linux"})
            QTest::newRow(platform) << QString(platform);
    }
    void switchesWorkWithPointerAndKeyboard() {
        QFETCH(QString, platform);
        AppWindow application(profile()); auto *window = application.window(); QVERIFY(window);
        window->setProperty("platformName", platform); window->setProperty("page", "settings");
        window->resize(1000, 1000); window->show(); QTest::qWait(30);
        auto *pause = window->findChild<QQuickItem *>("pauseSwitch"); QVERIFY(pause);
        auto *startup = window->findChild<QQuickItem *>("autostartSwitch"); QVERIFY(startup);
        QCOMPARE(startup->isVisible(), platform != "ios");
        const QPoint position = pause->mapToScene(QPointF(pause->width() - 24, pause->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, position);
        QTRY_VERIFY(application.controller()->paused());
        pause->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!application.controller()->paused());
        application.controller()->setPaused(true);
        QTRY_VERIFY(pause->property("checked").toBool());
    }
    void flickHasInertiaAndDoesNotBlockEventLoop() {
        AppController controller(profile()); QQmlApplicationEngine engine;
        configureAppEngine(engine, controller); engine.load(QUrl("qrc:/app/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        window->resize(360, 440); window->setProperty("page", "settings"); window->show(); QTest::qWait(50);
        auto *scroll = window->findChild<QQuickItem *>("settingsScroll"); QVERIFY(scroll);
        QVERIFY(scroll->property("contentHeight").toReal() > scroll->height());
        // Observe independent event delivery while inertia advances. Counting timer
        // callbacks in a fixed 100 ms window is scheduler-dependent on CI runners.
        QEventLoop observation;
        int movingTicks = 0;
        qreal lastY = scroll->property("contentY").toReal();
        QTimer timer;
        timer.setTimerType(Qt::PreciseTimer);
        connect(&timer, &QTimer::timeout, &observation, [&] {
            const auto currentY = scroll->property("contentY").toReal();
            if (scroll->property("flicking").toBool() && currentY > lastY + 1) {
                lastY = currentY;
                if (++movingTicks >= 3) observation.quit();
            }
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        watchdog.setTimerType(Qt::PreciseTimer);
        connect(&watchdog, &QTimer::timeout, &observation, &QEventLoop::quit);
        QElapsedTimer elapsed;
        elapsed.start();
        timer.start(16);
        watchdog.start(2000);
        QVERIFY(QMetaObject::invokeMethod(scroll, "flick", Q_ARG(qreal, 0.0), Q_ARG(qreal, -1800.0)));
        observation.exec();
        timer.stop();
        watchdog.stop();
        const auto diagnostic = QString("Observed %1 moving timer callbacks in %2 ms; contentY=%3, flicking=%4")
            .arg(movingTicks).arg(elapsed.elapsed()).arg(scroll->property("contentY").toReal())
            .arg(scroll->property("flicking").toBool());
        QVERIFY2(movingTicks >= 3, qPrintable(diagnostic));
        QVERIFY2(elapsed.elapsed() < 2000, qPrintable(diagnostic));
        QVERIFY(scroll->property("contentY").toReal() > 30);
        const auto y = scroll->property("contentY").toReal();
        controller.setLanguage(controller.language() == "ru" ? "en" : "ru");
        QTest::qWait(30);
        QVERIFY(scroll->property("contentY").toReal() >= y - 1);
    }
    void invalidRegistrationPreservesExistingProfile() {
        AppController controller(profile());
        const auto invitation = controller.invitation();
        QVERIFY(!controller.registerDevice("", "phone", false, ""));
        QCOMPARE(controller.invitation(), invitation);
        QVERIFY(!controller.registerDevice("owner", "phone", true, "https://example.com"));
        QCOMPARE(controller.invitation(), invitation);
        QVERIFY(!controller.message().isEmpty());
    }
    void receivedFilesRenderAndDeleteOnlyLocalCopies() {
        I18n::setLanguage("ru");
        const QString root = QDir(Profile::dataDirectory()).filePath("received");
        QVERIFY(QDir().mkpath(root + "/test-batch"));
        const QString path = root + "/test-batch/photo with a long name.png";
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("test"); file.close();
        QTemporaryDir outside;
        QFile external(outside.filePath("private.txt")); QVERIFY(external.open(QIODevice::WriteOnly)); external.write("private"); external.close();
        QVERIFY(QFile::link(external.fileName(), root + "/test-batch/link"));
        AppController controller(profile()); QQmlApplicationEngine engine;
        configureAppEngine(engine, controller);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.load(QUrl("qrc:/app/Main.qml")); QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        window->resize(320, 640);
        window->setProperty("page", "files"); window->show(); controller.refreshFiles();
        QTRY_VERIFY(!controller.loadingFiles());
        QCOMPARE(controller.receivedFiles().size(), 1);
        QTest::qWait(40); QCOMPARE(warnings.size(), 0);
        for (auto *item : window->findChildren<QQuickItem *>()) {
            if (!item->isVisible() || item->property("text").toString() != QString::fromUtf8("Удалить")) continue;
            // A localized destructive action must fit without splitting a word.
            QVERIFY(item->width() >= item->implicitWidth());
        }
        controller.deleteReceivedFile(-1); QVERIFY(file.exists());
        controller.deleteReceivedFile(0); QTRY_VERIFY(!file.exists());
        QVERIFY(external.exists());
        QTRY_VERIFY(!controller.loadingFiles());
        QDir(root).removeRecursively(); // Unique test-mode application data only.
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv); initializeAppUi();
    QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName("LocalClipboardMobileTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    MobileTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "MobileTests.moc"

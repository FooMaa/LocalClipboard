#include "MobileApp.h"
#include "MobileController.h"
#include "I18n.h"
#include <QApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
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
        QTest::addColumn<int>("width"); QTest::addColumn<QString>("language");
        for (int width : {320, 412, 760})
            for (const auto &language : {QStringLiteral("ru"), QStringLiteral("en")})
                QTest::newRow(qPrintable(QString("%1-%2").arg(width).arg(language))) << width << language;
    }
    void screensFitAndTranslate() {
        QFETCH(int, width); QFETCH(QString, language);
        I18n::setLanguage(language);
        MobileController controller(profile()); QQmlApplicationEngine engine;
        configureMobileEngine(engine, controller);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.load(QUrl("qrc:/mobile/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->resize(width, 740); window->show();
        for (const auto &page : {"home", "settings", "register", "pair", "files"}) {
            window->setProperty("page", page); QTest::qWait(40);
            const auto items = window->findChildren<QQuickItem *>();
            for (auto *item : items) {
                if (!item->isVisible() || item->width() == 0) continue;
                const auto wrap = item->property("wrapMode");
                if (wrap.isValid() && item->property("text").isValid()) {
                    QVERIFY2(item->width() <= width, qPrintable(item->property("text").toString()));
                    const auto format = item->property("textFormat");
                    // Untrusted device names and filenames must always be rendered as plain text.
                    if (format.isValid() && item->property("text").toString().contains(controller.deviceName()))
                        QCOMPARE(format.toInt(), 0);
                }
            }
            if (width == 412 && language == "ru" && QString(page) != "pair") {
                const auto image = window->grabWindow();
                QVERIFY(!image.isNull());
                image.save(QDir::current().filePath(QString("mobile-%1-ru.png").arg(page)));
            }
        }
        for (const auto &warning : warnings) {
            const auto errors = qvariant_cast<QList<QQmlError>>(warning.first());
            for (const auto &error : errors) qWarning().noquote() << error.toString();
        }
        QCOMPARE(warnings.size(), 0);
    }
    void flickHasInertiaAndDoesNotBlockEventLoop() {
        MobileController controller(profile()); QQmlApplicationEngine engine;
        configureMobileEngine(engine, controller); engine.load(QUrl("qrc:/mobile/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        window->resize(360, 440); window->setProperty("page", "settings"); window->show(); QTest::qWait(50);
        auto *scroll = window->findChild<QQuickItem *>("settingsScroll"); QVERIFY(scroll);
        QVERIFY(scroll->property("contentHeight").toReal() > scroll->height());
        int ticks = 0; QTimer timer; connect(&timer, &QTimer::timeout, this, [&] { ++ticks; }); timer.start(10);
        QVERIFY(QMetaObject::invokeMethod(scroll, "flick", Q_ARG(qreal, 0.0), Q_ARG(qreal, -1800.0)));
        QTRY_VERIFY(scroll->property("contentY").toReal() > 30);
        const auto first = scroll->property("contentY").toReal();
        QTest::qWait(100);
        QVERIFY(scroll->property("contentY").toReal() > first);
        QVERIFY(ticks >= 5);
        const auto y = scroll->property("contentY").toReal();
        controller.setLanguage(controller.language() == "ru" ? "en" : "ru");
        QTest::qWait(30);
        QVERIFY(scroll->property("contentY").toReal() >= y - 1);
    }
    void invalidRegistrationPreservesExistingProfile() {
        MobileController controller(profile());
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
        MobileController controller(profile()); QQmlApplicationEngine engine;
        configureMobileEngine(engine, controller);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.load(QUrl("qrc:/mobile/Main.qml")); QVERIFY(!engine.rootObjects().isEmpty());
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
    QApplication app(argc, argv); QQuickStyle::setStyle("Basic");
    QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName("LocalClipboardMobileTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    MobileTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "MobileTests.moc"

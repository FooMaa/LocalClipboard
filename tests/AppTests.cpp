#include "ClipboardSync.h"
#include "SyncEngine.h"
#include <QApplication>
#include <QClipboard>
#include <QTest>
#include <QUuid>

class AppTests final : public QObject {
    Q_OBJECT
private slots:
    void closingWindowKeepsSynchronizationAlive() {
        Profile profile;
        profile.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        profile.deviceName = "background-test"; profile.ownerName = "test"; profile.create();
        QApplication::clipboard()->clear();
        ClipboardSync window(profile);
        window.show();
        QTest::qWait(10);
        QVERIFY(window.isVisible());
        auto *engine = window.findChild<SyncEngine *>();
        QVERIFY(engine && engine->port());
        window.close();
        QVERIFY(!window.isVisible());
        QVERIFY(engine->port());
        window.showSettings();
        QVERIFY(window.isVisible());
        const QJsonObject payload{{"kind", "text"}, {"text", "copy again"}, {"html", ""}};
        engine->received(payload);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("copy again"));
        QApplication::clipboard()->clear();
        engine->received(payload);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("copy again"));
    }
};
QTEST_MAIN(AppTests)
#include "AppTests.moc"

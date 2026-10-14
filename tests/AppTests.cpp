#include "ClipboardSync.h"
#include "SyncEngine.h"
#include "Registration.h"
#include "I18n.h"
#include "Ui.h"
#include "QrCode.h"
#include <QListWidget>
#include <QScroller>
#include <QScrollBar>
#include <QTransform>
#include "Uninstall.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPushButton>
#include <QProcess>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <QUuid>

class AppTests final : public QObject {
    Q_OBJECT
    QTemporaryDir preferences;
private slots:
    void initTestCase() {
        QVERIFY(preferences.isValid());
        I18n::initialize("en", preferences.path()); Ui::initialize();
    }
    void languageIsPersistedAndUpdatesOpenDialog() {
        RegistrationDialog dialog(Profile::load());
        auto *picker = dialog.findChild<QComboBox *>("languagePicker"); QVERIFY(picker);
        picker->setCurrentIndex(0);
        QCOMPARE(dialog.windowTitle(), QString::fromUtf8("Регистрация устройства"));
        auto *owner = dialog.findChild<QLineEdit *>("ownerName"); owner->setText(QString::fromUtf8("Антон"));
        auto *mode = dialog.findChild<QComboBox *>("profileMode"); mode->setCurrentIndex(1);
        auto *code = dialog.findChild<QLineEdit *>("connectionCode"); code->setText("LC2.test");
        I18n::initialize({}, preferences.path()); QCOMPARE(I18n::language(), QStringLiteral("ru"));
        picker->setCurrentIndex(1);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Register this device"));
        QCOMPARE(owner->text(), QString::fromUtf8("Антон"));
        QCOMPARE(mode->currentIndex(), 1); QVERIFY(code->isEnabled()); QCOMPARE(code->text(), QStringLiteral("LC2.test"));
    }
    void registrationFitsNarrowScreens_data() {
        QTest::addColumn<QString>("language"); QTest::addColumn<int>("width"); QTest::addColumn<int>("pointSize");
        for (const auto &language : {QStringLiteral("en"), QStringLiteral("ru")})
            for (int width : {360, 520})
                for (int size : {13, 20})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(language).arg(width).arg(size))) << language << width << size;
    }
    void registrationFitsNarrowScreens() {
        QFETCH(QString, language); QFETCH(int, width); QFETCH(int, pointSize); I18n::setLanguage(language);
        QFont larger = qApp->font(); larger.setPointSize(pointSize);
        Profile profile = Profile::load(); profile.deviceName = QString(80, QChar('W')); profile.ownerName = QString(80, QChar(0x042F));
        RegistrationDialog dialog(profile); dialog.setFont(larger); dialog.resize(width, 600); dialog.show(); QTest::qWait(20);
        QCOMPARE(dialog.width(), width);
        auto *scroll = dialog.findChild<QScrollArea *>(); QVERIFY(scroll);
        QVERIFY(scroll->widget()->width() <= scroll->viewport()->width());
        for (auto *label : dialog.findChildren<QLabel *>()) {
            if (!label->isVisible() || label->text().isEmpty()) continue;
            QVERIFY2(label->height() >= label->heightForWidth(label->width()), qPrintable(label->text()));
            const QRect bounds = label->geometry();
            for (auto *input : dialog.findChildren<QLineEdit *>())
                if (input->parentWidget() == label->parentWidget()) QVERIFY(!bounds.intersects(input->geometry()));
        }
        auto *save = dialog.findChild<QPushButton *>("registerButton"); QVERIFY(save);
        const QRect buttonRect(save->mapTo(&dialog, QPoint(0, 0)), save->size()); QVERIFY(dialog.rect().contains(buttonRect));
        if (language == "ru" && width == 520 && pointSize == 13) dialog.grab().save("/tmp/localclipboard-registration-ru.png");
    }
    void invalidRegistrationStaysInWizard() {
        I18n::setLanguage("ru"); RegistrationDialog dialog(Profile::load()); dialog.show();
        auto *save = dialog.findChild<QPushButton *>("registerButton");
        dialog.findChild<QLineEdit *>("ownerName")->clear(); save->click();
        QVERIFY(!dialog.findChild<QLabel *>("registrationError")->text().isEmpty()); QVERIFY(dialog.isVisible());
        dialog.findChild<QLineEdit *>("ownerName")->setText("Test");
        dialog.findChild<QComboBox *>("profileMode")->setCurrentIndex(1);
        dialog.findChild<QLineEdit *>("connectionCode")->setText("LC2.short"); save->click();
        QVERIFY(dialog.findChild<QLabel *>("registrationError")->text().contains(QString::fromUtf8("полный")));
    }
    void mainWindowFitsNarrowScreens() {
        Profile profile = Profile::load(); profile.create();
        profile.deviceName = QString(80, QChar('W')); profile.ownerName = QString(80, QChar(0x042F));
        for (const auto &language : {QStringLiteral("ru"), QStringLiteral("en")}) {
            I18n::setLanguage(language);
            ClipboardSync window(profile); QFont large = window.font(); large.setPointSize(20); window.setFont(large);
            window.resize(360, 600); window.show(); QTest::qWait(20);
            QCOMPARE(window.width(), 360);
            auto *scroll = qobject_cast<QScrollArea *>(window.centralWidget()); QVERIFY(scroll);
            QVERIFY(scroll->widget()->width() <= scroll->viewport()->width());
            for (auto *label : window.findChildren<QLabel *>())
                if (label->isVisible() && !label->text().isEmpty())
                    QVERIFY2(label->height() >= label->heightForWidth(label->width()), qPrintable(label->text()));
        }
    }
    void connectionQrRoundTripAndInvalidInput() {
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
    }
    void invitationShowsScannableProfile() {
        Profile profile; profile.create();
        QString decoded, code;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(qApp->activeModalWidget());
            if (!dialog) return;
            auto *image = dialog->findChild<QLabel *>("invitationQr");
            auto *input = dialog->findChild<QLineEdit *>();
            if (image) decoded = QrCode::decodeInvitation(image->pixmap().toImage());
            if (input) code = input->text();
            dialog->reject();
        });
        showInvitation(profile);
        QCOMPARE(decoded, profile.invitation()); QCOMPARE(code, decoded);
    }
    void mobileChoicesSelectAndFormsScroll() {
        qApp->setProperty("touchUi", true);
        RegistrationDialog dialog(Profile::load());
        const bool maximizedForm = dialog.windowState().testFlag(Qt::WindowMaximized);
        auto *mode = dialog.findChild<QComboBox *>("profileMode");
        bool chose = false;
        QTimer::singleShot(0, [&] {
            auto *picker = qobject_cast<QDialog *>(qApp->activeModalWidget());
            auto *list = picker ? picker->findChild<QListWidget *>("touchChoiceList") : nullptr;
            if (!list) { if (picker) picker->reject(); return; }
            chose = true;
            const QRect target = list->visualItemRect(list->item(1));
            QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, target.center());
        });
        mode->showPopup();
        const int selected = mode->currentIndex();
        const bool codeEnabled = dialog.findChild<QLineEdit *>("connectionCode")->isEnabled();
        auto *scroll = dialog.findChild<QScrollArea *>();
        const bool gesture = QScroller::hasScroller(scroll->viewport());
        dialog.setWindowState(Qt::WindowNoState); dialog.resize(360, 400); dialog.show(); QTest::qWait(30);
        const bool canScroll = scroll->verticalScrollBar()->maximum() > 0;
        const bool clearButton = dialog.findChild<QLineEdit *>("ownerName")->isClearButtonEnabled();
        auto *owner = dialog.findChild<QLineEdit *>("ownerName"); owner->setFocus();
        QTest::keyClick(owner, Qt::Key_Return);
        const bool advancedFocus = qApp->focusWidget() != owner;
        const bool noSubmit = dialog.findChild<QLabel *>("registrationError")->text().isEmpty();
        Profile profile = Profile::load(); profile.create();
        ClipboardSync main(profile); Ui::showWindow(&main); QTest::qWait(10);
        const bool maximizedMain = main.windowState().testFlag(Qt::WindowMaximized);
        main.setWindowState(Qt::WindowNoState); main.resize(360, 400); QTest::qWait(20);
        auto *mainScroll = qobject_cast<QScrollArea *>(main.centralWidget());
        const bool mainCanScroll = mainScroll && QScroller::hasScroller(mainScroll->viewport()) && mainScroll->verticalScrollBar()->maximum() > 0;
        qApp->setProperty("touchUi", false);
        QVERIFY(chose); QCOMPARE(selected, 1); QVERIFY(codeEnabled);
        QVERIFY(gesture); QVERIFY(canScroll); QVERIFY(clearButton);
        QVERIFY(maximizedForm); QVERIFY(maximizedMain); QVERIFY(mainCanScroll);
        QVERIFY(advancedFocus); QVERIFY(noSubmit);
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
        window.showMinimized(); window.showSettings();
        QVERIFY(!window.isMinimized());
        const QJsonObject payload{{"kind", "text"}, {"text", "copy again"}, {"html", ""}};
        engine->received(payload);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("copy again"));
        QApplication::clipboard()->clear();
        engine->received(payload);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("copy again"));
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

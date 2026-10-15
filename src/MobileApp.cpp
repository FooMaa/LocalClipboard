#include "MobileApp.h"
#include "MobileController.h"
#include "I18n.h"
#include <QApplication>
#include <QAccessible>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QStyleHints>

namespace {
class InvitationImages final : public QQuickImageProvider {
public:
    explicit InvitationImages(MobileController *controller) : QQuickImageProvider(Image), m_controller(controller) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        const QImage image = m_controller->qrImage();
        if (size) *size = image.size();
        return image;
    }
    MobileController *m_controller;
};
}
void configureMobileEngine(QQmlApplicationEngine &engine, MobileController &controller) {
    engine.addImageProvider("invitation", new InvitationImages(&controller));
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}});
    QObject::connect(&I18n::instance(), &I18n::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
}
int runMobileApp(QApplication &app) {
    QQuickStyle::setStyle("Basic");
#ifdef Q_OS_ANDROID
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    // Qt 6.11's Android accessibility bridge activates controls from the Java UI
    // thread. Create their attached accessibility objects on the Qt GUI thread
    // during QML construction, before TalkBack/UI automation can activate them.
    QAccessible::setActive(true);
#endif
    const Profile profile = Profile::load();
#ifdef Q_OS_ANDROID
    // Migrate legacy plaintext keys on a normal app launch. Atomic saving preserves the old profile on error.
    QString protectionError;
    if (profile.registered()) profile.save(&protectionError);
#endif
    MobileController controller(profile);
#ifdef Q_OS_ANDROID
    if (!protectionError.isEmpty()) controller.report(protectionError);
#endif
    QQmlApplicationEngine engine;
    configureMobileEngine(engine, controller);
    engine.load(QUrl("qrc:/mobile/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
#ifdef Q_OS_ANDROID
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) window->showMaximized();
#endif
    return app.exec();
}

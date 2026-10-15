#include "AppUi.h"
#include "AppController.h"
#include "I18n.h"
#include "Ui.h"
#include <QQuickStyle>
#ifdef Q_OS_ANDROID
#include <QAccessible>
#include <QGuiApplication>
#include <QStyleHints>
#endif
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>

namespace {
class InvitationImages final : public QQuickImageProvider {
public:
    explicit InvitationImages(AppController *controller) : QQuickImageProvider(Image), m_controller(controller) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        const QImage image = m_controller->qrImage();
        if (size) *size = image.size();
        return image;
    }
    AppController *m_controller;
};
}
void initializeAppUi() {
    Ui::initialize();
    QQuickStyle::setStyle("Basic");
#ifdef Q_OS_ANDROID
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    // Create attached accessibility objects on the Qt GUI thread before Android
    // accessibility services activate controls from the Java UI thread.
    QAccessible::setActive(true);
#endif
}
void configureAppEngine(QQmlApplicationEngine &engine, AppController &controller, bool background) {
    engine.addImageProvider("invitation", new InvitationImages(&controller));
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}, {"startHidden", background}});
    QObject::connect(&I18n::instance(), &I18n::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
}

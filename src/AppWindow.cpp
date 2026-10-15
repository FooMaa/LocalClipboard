#include "AppWindow.h"
#include "AppUi.h"
#include "I18n.h"
#include <QApplication>
#include <QCloseEvent>
#include <QSignalBlocker>
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include <QMenu>
#include <QSystemTrayIcon>
#endif

AppWindow::AppWindow(const Profile &profile, bool background) : m_controller(profile, this) {
    configureAppEngine(m_engine, m_controller, background);
    m_engine.load(QUrl("qrc:/app/Main.qml"));
    if (m_engine.rootObjects().isEmpty()) return;
    m_window = qobject_cast<QQuickWindow *>(m_engine.rootObjects().first());
    if (!m_window) return;
    m_window->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    const auto safeArea = [this] {
        const auto margins = m_window->safeAreaMargins();
        m_window->setProperty("safeTop", margins.top());
        m_window->setProperty("safeBottom", margins.bottom());
    };
    connect(m_window, &QWindow::safeAreaMarginsChanged, this, safeArea);
    safeArea();
#endif
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    m_window->showMaximized();
#else
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QIcon icon = QApplication::windowIcon();
#ifdef Q_OS_MACOS
        icon = QIcon(); icon.addFile(":/icons/localclipboard-tray-32.png");
        icon.addFile(":/icons/localclipboard-tray-64.png"); icon.setIsMask(true);
#endif
        m_menu = std::make_unique<QMenu>();
        auto *open = m_menu->addAction(QString(), this, &AppWindow::showSettings);
        auto *pause = m_menu->addAction(QString()); pause->setCheckable(true);
        connect(pause, &QAction::toggled, &m_controller, &AppController::setPaused);
        m_menu->addSeparator();
        auto *quit = m_menu->addAction(QString(), qApp, &QCoreApplication::quit);
        m_tray = new QSystemTrayIcon(icon, this);
        m_tray->setContextMenu(m_menu.get());
        const auto update = [this, open, pause, quit] {
            open->setText(I18n::text("Open LocalClipboard"));
            pause->setText(I18n::text("Pause synchronization"));
            quit->setText(I18n::text("Quit"));
            const QSignalBlocker block(pause); pause->setChecked(m_controller.paused());
            m_tray->setToolTip("LocalClipboard — " + m_controller.ownerName());
        };
        connect(&m_controller, &AppController::profileChanged, this, update);
        connect(&I18n::instance(), &I18n::languageChanged, this, update);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showSettings();
        });
        update(); m_tray->show();
    }
#endif
}
AppWindow::~AppWindow() {
    // QWindow emits events after the QQuickWindow part has been destroyed.
    // Stop callbacks before the engine starts tearing down its root objects.
    if (m_window) {
        m_window->removeEventFilter(this);
        QObject::disconnect(m_window, nullptr, this, nullptr);
    }
    m_window.clear();
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (m_tray) m_tray->setContextMenu(nullptr);
#endif
}
void AppWindow::showSettings() {
    if (!m_window) return;
    if (m_window->visibility() == QWindow::Minimized) m_window->showNormal();
    else m_window->show();
    m_window->raise(); m_window->requestActivate();
}
bool AppWindow::eventFilter(QObject *object, QEvent *event) {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (event->type() == QEvent::Close && object == m_window) {
        static_cast<QCloseEvent *>(event)->ignore();
        m_window->hide(); return true;
    }
#endif
    return QObject::eventFilter(object, event);
}

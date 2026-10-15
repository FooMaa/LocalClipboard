#pragma once
#include "AppController.h"
#include <QQmlApplicationEngine>
#include <QPointer>
#include <QQuickWindow>
#include <memory>
class QMenu;
class QSystemTrayIcon;

class AppWindow final : public QObject {
    Q_OBJECT
public:
    AppWindow(const Profile &profile, bool background = false);
    ~AppWindow() override;
    QQuickWindow *window() const { return m_window; }
    AppController *controller() { return &m_controller; }
    void showSettings();
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    AppController m_controller;
    QQmlApplicationEngine m_engine;
    QPointer<QQuickWindow> m_window;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    std::unique_ptr<QMenu> m_menu;
    QSystemTrayIcon *m_tray = nullptr;
#endif
};

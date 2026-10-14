#pragma once
#include "Profile.h"
#include <QJsonObject>
#include <QMainWindow>
class QCheckBox;
class QCloseEvent;
class QLabel;
class QListWidget;
class QSystemTrayIcon;
class SyncEngine;

class ClipboardSync final : public QMainWindow {
    Q_OBJECT
public:
    explicit ClipboardSync(const Profile &profile, QWidget *parent = nullptr);
    bool hasTray() const;
public slots:
    void showSettings();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void sendClipboard(bool explicitSend = false);
    void sendPayload(const QJsonObject &payload);
    void apply(const QJsonObject &payload);
    void refresh();
    void log(const QString &message);
    void startEngine();
    Profile m_profile;
    SyncEngine *m_engine = nullptr;
    QListWidget *m_peers = nullptr;
    QLabel *m_status = nullptr;
    QCheckBox *m_pause = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_lastHash;
    bool m_applying = false;
};

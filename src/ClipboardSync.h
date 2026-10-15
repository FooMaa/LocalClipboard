#pragma once
#include "Profile.h"
#include <QJsonObject>
#include <QMainWindow>
#include <QUrl>
#include <functional>
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
    void importFiles(const QList<QUrl> &urls);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void sendClipboard(bool explicitSend = false);
    void sendPayload(const QJsonObject &payload);
    void apply(const QJsonObject &payload);
    void refresh();
    void log(const QString &message);
    void startEngine();
    void networkSettings();
    void pollAndroidInbox();
    Profile m_profile;
    SyncEngine *m_engine = nullptr;
    QListWidget *m_peers = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_log = nullptr;
    QCheckBox *m_pause = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_lastHash;
    bool m_applying = false;
    bool m_loadingInbox = false;
    bool m_preparingSend = false;
    QStringList m_visiblePeers;
    std::function<void()> m_pendingApply;
};

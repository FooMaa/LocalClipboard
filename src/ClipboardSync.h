#pragma once
#include "Profile.h"
#include <QMainWindow>
#include <QUrl>
class QCheckBox;
class QCloseEvent;
class QLabel;
class QListWidget;
class QSystemTrayIcon;
class ClipboardSession;

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
    void refresh();
    void networkSettings();
    ClipboardSession *m_session = nullptr;
    QListWidget *m_peers = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_log = nullptr;
    QCheckBox *m_pause = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QStringList m_visiblePeers;
};

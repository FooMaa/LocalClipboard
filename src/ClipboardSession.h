#pragma once
#include "Profile.h"
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <functional>

class SyncEngine;

// Clipboard/service coordination is independent of the Widgets and Quick views.
class ClipboardSession final : public QObject {
    Q_OBJECT
public:
    explicit ClipboardSession(const Profile &profile, QObject *parent = nullptr);
    const Profile &profile() const { return m_profile; }
    QStringList peers() const { return m_peers; }
    QString message() const { return m_message; }
    bool busy() const { return m_preparingSend; }
    QString sharedDescription() const;
    void finishShare(bool send);
    bool receiving() const;
    void replaceProfile(const Profile &profile);
    bool saveProfile(const Profile &profile);
    bool setPaused(bool paused);
    bool setAutostart(bool enabled);
    void sendClipboard(bool explicitSend = true);
    void sendFiles(const QList<QUrl> &urls);
    void importFiles(const QList<QUrl> &urls);
    void report(const QString &message);
signals:
    void stateChanged();
    void profileChanged();
    void messageChanged();
    void busyChanged();
    void shareChanged();
private:
    void startEngine();
    void refresh();
    void pollAndroidInbox();
    void sendPayload(const QJsonObject &payload);
    void apply(const QJsonObject &payload);
    void setBusy(bool busy);
    Profile m_profile;
    SyncEngine *m_engine = nullptr;
    QStringList m_peers;
    QString m_message, m_lastHash;
    QJsonObject m_sharedPayload;
    quint64 m_revision = 0;
    bool m_applying = false, m_loadingInbox = false, m_preparingSend = false;
    bool m_loadingPeers = false, m_loadingShares = false;
    std::function<void()> m_pendingApply;
};

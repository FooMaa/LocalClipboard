#pragma once
#include "Profile.h"
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <functional>
#include <QVariantList>

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
    bool pendingClipboard() const;
    void copyReceivedClipboard();
    void replaceProfile(const Profile &profile);
    bool saveProfile(const Profile &profile);
    bool setPaused(bool paused);
    bool setAutostart(bool enabled);
    void sendClipboard(bool explicitSend = true);
    bool sendText(const QString &text);
    void copyExchangeItem(const QString &id);
    void sendFiles(const QList<QUrl> &urls);
    void importFiles(const QList<QUrl> &urls);
    void report(const QString &message);
    QVariantList devices() const;
    QVariantList transfers() const;
    bool authorizeDevice(const QString &id, bool revoke);
    bool retryTransfer(const QString &id);
    void cancelTransfer(const QString &id);
    void copyHistory(const QString &id);
    void sendReceivedFile(const QUrl &url) { sendFiles({url}); }
    void announce();
signals:
    void stateChanged();
    void profileChanged();
    void messageChanged();
    void busyChanged();
    void shareChanged();
    void transfersChanged();
    void historyChanged();
    void exchangeChanged();
private:
    void startEngine();
    void refresh();
    void copyPayload(const std::function<QJsonObject()> &load);
    bool sendPayload(const QJsonObject &payload);
    void apply(const QJsonObject &payload);
    void setBusy(bool busy);
    void completeReceived(const QString &state, const QString &error = {});
    void remember(const QJsonObject &payload);
    QString m_incomingId, m_incomingSender, m_serviceMessage;
    QVariantList m_serviceDevices, m_serviceTransfers;
    Profile m_profile;
    SyncEngine *m_engine = nullptr;
    QStringList m_peers;
    QString m_message, m_lastHash;
    QJsonObject m_sharedPayload;
    quint64 m_revision = 0;
    bool m_applying = false, m_preparingSend = false;
    bool m_loadingPeers = false, m_loadingShares = false;
    bool m_pendingInbox = false;
};

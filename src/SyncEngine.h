#pragma once
#include "Profile.h"
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QUdpSocket>
#include <QPointer>
#include <QVariantList>
class QTcpSocket;

class SyncEngine final : public QObject {
    Q_OBJECT
public:
    explicit SyncEngine(const Profile &profile, QObject *parent = nullptr,
                        quint16 discoveryPort = 45871, quint16 transferPort = 45872);
    ~SyncEngine() override;
    bool send(const QJsonObject &payload);
    void setPaused(bool paused);
    void configure(const Profile &profile);
    void announce();
    void contact(const QHostAddress &address, quint16 discoveryPort = 45871);
    void contactTcp(const QHostAddress &address, quint16 transferPort = 45872);
    quint16 port() const { return m_server.serverPort(); }
    quint16 discoveryPort() const { return m_udp.localPort(); }
    QStringList peerDescriptions() const;
    QVariantList devices() const;
    QVariantList transfers() const;
    bool authorizeDevice(const QString &id, bool revoke);
    void completeIncoming(const QString &id, const QString &sender, const QString &state, const QString &error = {});
    bool retry(const QString &id);
    void cancel(const QString &id);
    const Profile &profile() const { return m_profile; }
signals:
    void received(const QJsonObject &payload);
    void peersChanged();
    void transfersChanged();
    void registryChanged();
    void incomingTransfer(const QString &id, const QString &sender);
    void message(const QString &text);
private:
    struct Peer { QString id, name; QHostAddress address; quint16 port; qint64 seen; bool receipts = false; QByteArray publicKey; quint16 discoveryPort = 45871; };
    // disconnected can be emitted from QAbstractSocket's destructor, after the
    // QTcpSocket subobject is gone but before QObject clears guarded pointers.
    using SocketGuard = QPointer<QAbstractSocket>;
    struct Job { QString id, peer, name, state, error, description; int progress = 0; qint64 time = 0; SocketGuard socket; QString kind; int items = 0; };
    void updateJob(const QString &id, const QString &state, int progress, const QString &error = {});
    bool sendControl(const Peer &peer, const QJsonObject &control);
    void sendPendingReceipts();
    void loadDevices();
    void saveDevices();
    void discover();
    bool processAnnouncement(const QByteArray &bytes, const QHostAddress &address, quint16 sourcePort, QTcpSocket *discoverySocket = nullptr);
    void writeAnnouncement(const QByteArray &bytes, const QHostAddress &address, quint16 port);
    void accept();
    bool sendTo(const Peer &peer, const QByteArray &plain, const QString &jobId = {});
    void drainOutgoing();
    Profile m_profile;
    quint16 m_discoveryPort;
    QUdpSocket m_udp;
    QTcpServer m_server;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QHash<QString, Peer> m_peers;
    QHash<QString, SocketGuard> m_discoveryProbes;
    qint64 m_lastDiscoveryError = -15000;
    int m_connections = 0;
    qint64 m_bufferedBytes = 0;
    QByteArray m_pendingPlain;
    QList<Peer> m_pendingPeers;
    bool m_stopping = false;
    QHash<QString, Job> m_jobs;
    QHash<QString, QJsonObject> m_knownDevices;
    QJsonObject m_retryPayload;
    QString m_batchId;
    QHash<QString, QJsonObject> m_receipts;
    QHash<QString, qint64> m_receiptAttempts;
    struct IncomingAck { SocketGuard socket; Profile channel; QByteArray context; };
    QHash<QString, IncomingAck> m_incomingAcks;
    QHash<QString, QString> m_completedIncoming;
    QStringList m_completedOrder;
};

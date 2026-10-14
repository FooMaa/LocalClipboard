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

class SyncEngine final : public QObject {
    Q_OBJECT
public:
    explicit SyncEngine(const Profile &profile, QObject *parent = nullptr,
                        quint16 discoveryPort = 45871, quint16 transferPort = 45872);
    ~SyncEngine() override;
    bool send(const QJsonObject &payload);
    void setPaused(bool paused) { m_profile.paused = paused; }
    void announce();
    quint16 port() const { return m_server.serverPort(); }
    quint16 discoveryPort() const { return m_udp.localPort(); }
    QStringList peerDescriptions() const;
signals:
    void received(const QJsonObject &payload);
    void peersChanged();
    void message(const QString &text);
private:
    struct Peer { QString id, name; QHostAddress address; quint16 port; qint64 seen; };
    void discover();
    void accept();
    void sendTo(const Peer &peer, const QByteArray &plain);
    Profile m_profile;
    quint16 m_discoveryPort;
    QUdpSocket m_udp;
    QTcpServer m_server;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QHash<QString, Peer> m_peers;
    int m_connections = 0;
};

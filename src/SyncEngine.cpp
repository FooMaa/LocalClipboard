#include "SyncEngine.h"
#include "ClipboardPayload.h"
#include "SecureProtocol.h"
#include <QDataStream>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QtEndian>
#include <QUuid>
#include <memory>

namespace {
constexpr int HelloBytes = 104;
constexpr int ProofBytes = 68;
constexpr qint64 TransferMemoryBudget = SecureProtocol::MaxWireBytes * 2LL;
struct IncomingTransfer {
    enum Phase { Proof, Length, Payload, Done } phase = Proof;
    QByteArray sender;
    quint32 length = 0;
};
QByteArray serverProof(const QByteArray &challenge, const QByteArray &receiver) { return QByteArray("server/") + challenge + receiver; }
QByteArray clientProof(const QByteArray &challenge, const QByteArray &sender, const QByteArray &receiver) { return QByteArray("client/") + challenge + sender + receiver; }
QByteArray context(const QByteArray &challenge, const QByteArray &sender, const QByteArray &receiver) { return QByteArray("LocalClipboard/v2/frame/") + challenge + sender + receiver; }
QTimer *deadline(QTcpSocket *socket) {
    auto *timer = new QTimer(socket);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, socket, [socket] { socket->abort(); socket->deleteLater(); });
    timer->start(5000); // Authentication must finish before reserving a file buffer.
    return timer;
}
}
SyncEngine::SyncEngine(const Profile &profile, QObject *parent, quint16 discoveryPort, quint16 transferPort)
    : QObject(parent), m_profile(profile), m_discoveryPort(discoveryPort) {
    m_clock.start();
    if (!m_profile.registered()) return;
    if (!m_udp.bind(QHostAddress::AnyIPv4, discoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        QTimer::singleShot(0, this, [this] { emit message("Discovery could not start: " + m_udp.errorString()); });
    if (!m_server.listen(QHostAddress::AnyIPv4, transferPort)
        && !(m_server.serverError() == QAbstractSocket::AddressInUseError && m_server.listen(QHostAddress::AnyIPv4, 0)))
        QTimer::singleShot(0, this, [this] { emit message("Transfer port is in use or blocked: " + m_server.errorString()); });
    m_server.setMaxPendingConnections(8);
    connect(&m_udp, &QUdpSocket::readyRead, this, &SyncEngine::discover);
    connect(&m_server, &QTcpServer::newConnection, this, &SyncEngine::accept);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        for (auto it = m_peers.begin(); it != m_peers.end();) {
            if (m_clock.elapsed() - it->seen > 15000) it = m_peers.erase(it); else ++it;
        }
        announce();
    });
    m_timer.start(3000);
    QTimer::singleShot(0, this, &SyncEngine::announce);
}
SyncEngine::~SyncEngine() {
    m_stopping = true;
    // Dispose of sockets while counters and profile still exist (also when switching profiles).
    const auto sockets = findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *socket : sockets) delete socket;
}
void SyncEngine::setPaused(bool paused) {
    m_profile.paused = paused;
    if (!paused) return;
    m_pendingPlain.clear(); m_pendingPeers.clear();
    for (auto *socket : findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly)) socket->abort();
}
void SyncEngine::announce() {
    if (!m_profile.registered() || !m_server.isListening()) return;
    if (m_discoveryPort == 0) { emit peersChanged(); return; }
    const QByteArray bytes = SecureProtocol::announcement(m_profile, port());
#ifndef Q_OS_IOS
    m_udp.writeDatagram(bytes, QHostAddress::Broadcast, m_discoveryPort);
    // Directed broadcasts reach each active LAN when VPNs or multiple adapters are present.
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        if (!(interface.flags() & QNetworkInterface::IsUp) || (interface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull())
                m_udp.writeDatagram(bytes, entry.broadcast(), m_discoveryPort);
        }
    }
#endif
    for (const QString &address : m_profile.peerAddresses) contact(QHostAddress(address), m_discoveryPort);
    // A phone can initiate contact without receiving any broadcast traffic.
    for (const auto &peer : m_peers) contact(peer.address, m_discoveryPort);
    emit peersChanged();
}
void SyncEngine::contact(const QHostAddress &address, quint16 discoveryPort) {
    if (!m_profile.registered() || !m_server.isListening() || !discoveryPort
        || address.protocol() != QAbstractSocket::IPv4Protocol || address.isNull()
        || address.isMulticast() || address == QHostAddress::Broadcast) return;
    m_udp.writeDatagram(SecureProtocol::announcement(m_profile, port()), address, discoveryPort);
}
void SyncEngine::discover() {
    int processed = 0;
    while (m_udp.hasPendingDatagrams() && processed++ < 64) {
        if (m_udp.pendingDatagramSize() > 2048) { m_udp.readDatagram(nullptr, 0); continue; }
        QByteArray bytes(int(m_udp.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress address;
        quint16 sourcePort = 0;
        if (m_udp.readDatagram(bytes.data(), bytes.size(), &address, &sourcePort) < 0) continue;
        QJsonObject obj;
        if (!SecureProtocol::readAnnouncement(m_profile, bytes, &obj)) continue;
        const QString id = obj.value("id").toString();
        if (id == m_profile.deviceId || (m_peers.size() >= 64 && !m_peers.contains(id))) continue;
        const bool isNew = !m_peers.contains(id) || m_peers.value(id).address != address;
        m_peers.insert(id, Peer{id, obj.value("name").toString(), address, quint16(obj.value("port").toInt()), m_clock.elapsed()});
        // Reply once to a newly authenticated peer; never create a reply loop.
        if (isNew) contact(address, sourcePort);
        emit peersChanged();
    }
    if (m_udp.hasPendingDatagrams()) QTimer::singleShot(0, this, &SyncEngine::discover);
}
QStringList SyncEngine::peerDescriptions() const {
    QStringList peers;
    for (const auto &p : m_peers) peers.append(QStringLiteral("%1 — %2").arg(p.name, p.address.toString()));
    peers.sort();
    return peers;
}
bool SyncEngine::send(const QJsonObject &payload) {
    QString error;
    if (m_profile.paused || !ClipboardPayload::validate(payload, &error)) return false;
    const QByteArray plain = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (plain.size() > SecureProtocol::MaxWireBytes - 40 || m_peers.isEmpty()) return false;
    if (!m_pendingPeers.isEmpty()) { emit message("Transfer queue is busy; try again."); return false; }
    m_pendingPlain = plain;
    m_pendingPeers = m_peers.values();
    drainOutgoing();
    emit message("Sending " + ClipboardPayload::description(payload));
    return true;
}
void SyncEngine::drainOutgoing() {
    if (m_profile.paused || m_stopping) return;
    const qint64 reservation = m_pendingPlain.size() + 40;
    while (!m_pendingPeers.isEmpty() && m_connections < 8 && reservation <= TransferMemoryBudget - m_bufferedBytes) {
        const Peer peer = m_pendingPeers.takeFirst();
        sendTo(peer, m_pendingPlain);
    }
    if (m_pendingPeers.isEmpty()) m_pendingPlain.clear();
}
bool SyncEngine::sendTo(const Peer &peer, const QByteArray &plain) {
    const qint64 reservation = plain.size() + 40;
    if (m_connections >= 8 || reservation > TransferMemoryBudget - m_bufferedBytes) {
        emit message("Transfer queue is busy; try again."); return false;
    }
    auto *socket = new QTcpSocket(this);
    m_bufferedBytes += reservation;
    ++m_connections;
    connect(socket, &QObject::destroyed, this, [this, reservation] {
        --m_connections; m_bufferedBytes -= reservation;
        if (!m_stopping) QTimer::singleShot(0, this, &SyncEngine::drainOutgoing);
    });
    socket->setReadBufferSize(HelloBytes);
    auto *timeout = deadline(socket);
    const auto sent = std::make_shared<bool>(false);
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        emit message("Transfer failed: " + socket->errorString()); socket->deleteLater();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, timeout, sent, peer, plain] {
        if (m_profile.paused) { socket->abort(); return; }
        if (*sent || socket->bytesAvailable() < HelloBytes) return;
        const QByteArray hello = socket->read(HelloBytes);
        const QByteArray challenge = hello.mid(4, 32), receiver = hello.mid(36, 36);
        if (hello.left(4) != "LC2!" || receiver != peer.id.toUtf8()
            || !SecureProtocol::verify(hello.mid(72, 32), SecureProtocol::mac(m_profile, serverProof(challenge, receiver), "handshake"))) {
            socket->abort(); return;
        }
        *sent = true; timeout->start(30000);
        const QByteArray sender = m_profile.deviceId.toUtf8();
        const QByteArray wire = SecureProtocol::seal(m_profile, plain, context(challenge, sender, receiver));
        if (wire.isEmpty()) { socket->abort(); return; }
        QByteArray header(4, Qt::Uninitialized);
        qToBigEndian(quint32(wire.size()), reinterpret_cast<uchar *>(header.data()));
        socket->write(sender + SecureProtocol::mac(m_profile, clientProof(challenge, sender, receiver), "handshake") + header + wire);
        socket->disconnectFromHost();
    });
    socket->connectToHost(peer.address, peer.port);
    return true;
}
void SyncEngine::accept() {
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        socket->setParent(this);
        if (m_connections >= 8 || m_profile.paused) { socket->abort(); socket->deleteLater(); continue; }
        ++m_connections;
        connect(socket, &QObject::destroyed, this, [this] {
            --m_connections;
            if (!m_stopping) QTimer::singleShot(0, this, &SyncEngine::drainOutgoing);
        });
        // Unauthenticated connections can buffer only the identity and proof, never a file payload.
        socket->setReadBufferSize(ProofBytes);
        const QByteArray challenge = SecureProtocol::randomBytes(32), receiver = m_profile.deviceId.toUtf8();
        socket->write(QByteArray("LC2!") + challenge + receiver + SecureProtocol::mac(m_profile, serverProof(challenge, receiver), "handshake"));
        auto *timeout = deadline(socket);
        const auto transfer = std::make_shared<IncomingTransfer>();
        connect(socket, &QObject::destroyed, this, [this, transfer] { m_bufferedBytes -= transfer->length; });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::errorOccurred, socket, [socket](QAbstractSocket::SocketError) { socket->deleteLater(); });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, timeout, transfer, challenge, receiver] {
            if (m_profile.paused) { socket->abort(); return; }
            if (transfer->phase == IncomingTransfer::Done) return;
            if (transfer->phase == IncomingTransfer::Proof) {
                if (socket->bytesAvailable() < ProofBytes) return;
                const QByteArray proof = socket->read(ProofBytes), sender = proof.left(36);
                const QUuid id(QString::fromUtf8(sender));
                if (id.isNull() || id.toString(QUuid::WithoutBraces).toUtf8() != sender || sender == receiver
                    || !SecureProtocol::verify(proof.mid(36), SecureProtocol::mac(m_profile, clientProof(challenge, sender, receiver), "handshake"))) {
                    socket->abort(); return;
                }
                transfer->sender = sender;
                transfer->phase = IncomingTransfer::Length;
                socket->setReadBufferSize(4);
                timeout->start(30000);
            }
            if (transfer->phase == IncomingTransfer::Length) {
                if (socket->bytesAvailable() < 4) return;
                const QByteArray header = socket->read(4);
                const quint32 length = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(header.constData()));
                if (length < 40 || length > SecureProtocol::MaxWireBytes
                    || length > TransferMemoryBudget - m_bufferedBytes) { socket->abort(); return; }
                transfer->length = length;
                m_bufferedBytes += length;
                transfer->phase = IncomingTransfer::Payload;
                socket->setReadBufferSize(length);
            }
            const quint32 length = transfer->length;
            if (socket->bytesAvailable() < length) return;
            transfer->phase = IncomingTransfer::Done;
            QByteArray plain;
            if (!m_profile.paused && SecureProtocol::open(m_profile, socket->read(length),
                context(challenge, transfer->sender, receiver), &plain)) {
                const QJsonObject payload = QJsonDocument::fromJson(plain).object();
                QString error;
                if (ClipboardPayload::validate(payload, &error)) {
                    emit received(payload);
                    emit message("Received " + ClipboardPayload::description(payload));
                } else emit message("Rejected clipboard payload: " + error);
            }
            socket->disconnectFromHost();
        });
    }
}

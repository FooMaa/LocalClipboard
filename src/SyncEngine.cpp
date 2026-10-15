#include "SyncEngine.h"
#include "ClipboardPayload.h"
#include "DeviceAccess.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include "SecureProtocol.h"
#include <QDataStream>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QtEndian>
#include <QUuid>
#include <memory>
#include <algorithm>

namespace {
constexpr int HelloBytes = 104;
constexpr int ProofBytes = 68;
constexpr qint64 TransferMemoryBudget = SecureProtocol::MaxWireBytes * 2LL;
struct IncomingTransfer {
    enum Phase { Proof, Discovery, Length, Payload, Done } phase = Proof;
    QByteArray sender;
    Profile channel;
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
    loadDevices();
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
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        for (auto it = m_receipts.begin(); it != m_receipts.end();) {
            if (now - qint64(it.value().value("time").toDouble()) > 600) {
                m_receiptAttempts.remove(it.key()); it = m_receipts.erase(it);
            } else ++it;
        }
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
    for (auto *socket : sockets) {
        // A socket's base destructor may emit disconnected. No transfer updates
        // or queue bookkeeping are needed while the engine itself is shutting down.
        QObject::disconnect(socket, nullptr, this, nullptr);
        delete socket;
    }
}
void SyncEngine::setPaused(bool paused) {
    m_profile.paused = paused;
    if (!paused) return;
    m_pendingPlain.clear(); m_pendingPeers.clear();
    for (auto *socket : findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly)) socket->abort();
}
void SyncEngine::announce() {
    if (!m_profile.registered() || !m_server.isListening()) return;
    const QByteArray bytes = SecureProtocol::announcement(m_profile, port());
#ifndef Q_OS_IOS
    if (m_discoveryPort) writeAnnouncement(bytes, QHostAddress::Broadcast, m_discoveryPort);
    // Directed broadcasts reach each active LAN when VPNs or multiple adapters are present.
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        if (!(interface.flags() & QNetworkInterface::IsUp) || (interface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull())
                if (m_discoveryPort) writeAnnouncement(bytes, entry.broadcast(), m_discoveryPort);
        }
    }
#endif
    for (const QString &address : m_profile.peerAddresses) {
        const QHostAddress endpoint(address);
        if (m_discoveryPort) contact(endpoint, m_discoveryPort);
        bool recentlySeen = false;
        for (const auto &peer : m_peers)
            if (peer.address == endpoint && m_clock.elapsed() - peer.seen < 6000) recentlySeen = true;
        if (!recentlySeen) contactTcp(endpoint);
    }
    // A phone can initiate contact without receiving any broadcast traffic.
    for (const auto &peer : m_peers) contact(peer.address, peer.discoveryPort);
    emit peersChanged();
}
void SyncEngine::contact(const QHostAddress &address, quint16 discoveryPort) {
    if (!m_profile.registered() || !m_server.isListening() || !discoveryPort
        || address.protocol() != QAbstractSocket::IPv4Protocol || address.isNull()
        || address.isMulticast() || address == QHostAddress::Broadcast) return;
    writeAnnouncement(SecureProtocol::announcement(m_profile, port()), address, discoveryPort);
}
void SyncEngine::writeAnnouncement(const QByteArray &bytes, const QHostAddress &address, quint16 port) {
    if (m_udp.writeDatagram(bytes, address, port) >= 0 || m_clock.elapsed() - m_lastDiscoveryError < 15000) return;
    m_lastDiscoveryError = m_clock.elapsed();
    emit message(m_udp.error() == QAbstractSocket::SocketAccessError
        ? QStringLiteral("Local network access is blocked. Allow LocalClipboard in system privacy settings and check the firewall.")
        : QStringLiteral("Discovery send failed: ") + m_udp.errorString());
}
void SyncEngine::contactTcp(const QHostAddress &address, quint16 transferPort) {
    const QString endpoint = address.toString();
    if (!m_profile.registered() || !m_server.isListening() || !transferPort || m_connections >= 4
        || m_discoveryProbes.contains(endpoint) || address.protocol() != QAbstractSocket::IPv4Protocol
        || address.isNull() || address.isMulticast() || address == QHostAddress::Broadcast) return;
    auto *socket = new QTcpSocket(this);
    m_discoveryProbes.insert(endpoint, socket); ++m_connections;
    connect(socket, &QObject::destroyed, this, [this, endpoint] {
        m_discoveryProbes.remove(endpoint); --m_connections;
        if (!m_stopping) QTimer::singleShot(0, this, &SyncEngine::drainOutgoing);
    });
    socket->setReadBufferSize(HelloBytes);
    auto *timeout = deadline(socket);
    const auto sent = std::make_shared<bool>(false);
    const auto length = std::make_shared<quint32>(0);
    const auto receiver = std::make_shared<QString>();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::errorOccurred, socket, [this, socket](QAbstractSocket::SocketError error) {
        if (error == QAbstractSocket::SocketAccessError && m_clock.elapsed() - m_lastDiscoveryError >= 15000) {
            m_lastDiscoveryError = m_clock.elapsed();
            emit message("Local network access is blocked. Allow LocalClipboard in system privacy settings and check the firewall.");
        }
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, timeout, address, sent, length, receiver] {
        if (!*sent) {
            if (socket->bytesAvailable() < HelloBytes) return;
            const auto hello = socket->read(HelloBytes);
            const auto id = hello.mid(36, 36);
            if (hello.left(4) != "LC2!" || QUuid(QString::fromUtf8(id)).isNull() || id == m_profile.deviceId.toUtf8()
                || !SecureProtocol::verify(hello.mid(72, 32), SecureProtocol::mac(m_profile, serverProof(hello.mid(4, 32), id), "handshake"))) {
                socket->abort(); return;
            }
            *receiver = QString::fromUtf8(id); *sent = true;
            const auto announcement = SecureProtocol::announcement(m_profile, port());
            QByteArray header("LCD?"); header.resize(8);
            qToBigEndian(quint32(announcement.size()), reinterpret_cast<uchar *>(header.data() + 4));
            socket->setReadBufferSize(32772); socket->write(header + announcement);
        }
        if (!*length) {
            if (socket->bytesAvailable() < 4) return;
            const auto header = socket->read(4);
            *length = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(header.constData()));
            if (!*length || *length > 32768) { socket->abort(); return; }
        }
        if (socket->bytesAvailable() < *length) return;
        const auto bytes = socket->read(*length);
        QJsonObject announcement;
        if (SecureProtocol::readAnnouncement(m_profile, bytes, &announcement) && announcement.value("id").toString() == *receiver)
            processAnnouncement(bytes, address, 45871, socket);
        timeout->stop(); socket->disconnectFromHost();
    });
    socket->connectToHost(address, transferPort);
}
void SyncEngine::discover() {
    int processed = 0;
    while (m_udp.hasPendingDatagrams() && processed++ < 64) {
        if (m_udp.pendingDatagramSize() > 32768) { m_udp.readDatagram(nullptr, 0); continue; }
        QByteArray bytes(int(m_udp.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress address;
        quint16 sourcePort = 0;
        if (m_udp.readDatagram(bytes.data(), bytes.size(), &address, &sourcePort) < 0) continue;
        processAnnouncement(bytes, address, sourcePort);
    }
    if (m_udp.hasPendingDatagrams()) QTimer::singleShot(0, this, &SyncEngine::discover);
}
bool SyncEngine::processAnnouncement(const QByteArray &bytes, const QHostAddress &address, quint16 sourcePort, QTcpSocket *discoverySocket) {
    QJsonObject obj;
    if (!SecureProtocol::readAnnouncement(m_profile, bytes, &obj)) return false;
    const QString id = obj.value("id").toString();
    if (id == m_profile.deviceId || (m_peers.size() >= 64 && !m_peers.contains(id))) return false;
    const auto publicKey = QByteArray::fromHex(obj.value("publicKey").toString().toLatin1());
    if (m_profile.managed()) {
        Profile next = m_profile;
        if (DeviceAccess::updateRegistry(&next, obj.value("registry").toObject())) {
            QString error;
            // Merge into the latest local preferences (Android's UI uses a separate process).
            Profile saved = Profile::load();
            if (saved.key == next.key && saved.authorityPublic == next.authorityPublic) {
                saved.registry = next.registry;
                if (!saved.save(&error)) { emit message(error); return false; }
            }
            m_profile.registry = next.registry; emit registryChanged();
            for (auto *active : findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly)) if (active != discoverySocket) active->abort();
        }
    }
    auto metadata = m_knownDevices.value(id);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const bool metadataChanged = metadata.value("name").toString() != obj.value("name").toString()
        || now - qint64(metadata.value("seen").toDouble()) >= 10;
    metadata.insert("id", id); metadata.insert("name", obj.value("name"));
    metadata.insert("key", QString::fromLatin1(publicKey.toHex())); metadata.insert("seen", double(now));
    if (m_knownDevices.size() < 64 || m_knownDevices.contains(id)) m_knownDevices.insert(id, metadata);
    if (metadataChanged) saveDevices();
    const bool isNew = !m_peers.contains(id) || m_peers.value(id).address != address;
    m_peers.insert(id, Peer{id, obj.value("name").toString(), address, quint16(obj.value("port").toInt()), m_clock.elapsed(), obj.value("receipts").toBool(), publicKey, sourcePort});
    // Reply once to a newly authenticated peer; never create a reply loop.
    if (isNew && !discoverySocket) contact(address, sourcePort);
    // A receipt queued while the phone was suspended is delivered on reconnection.
    if (DeviceAccess::approved(m_profile, id, publicKey)) {
        const auto receipts = m_receipts;
        for (auto it = receipts.cbegin(); it != receipts.cend(); ++it) {
            if (now - qint64(it.value().value("time").toDouble()) > 600) { m_receipts.remove(it.key()); m_receiptAttempts.remove(it.key()); continue; }
            if (it.value().value("to").toString() == id && now - m_receiptAttempts.value(it.key()) >= 3
                && sendControl(m_peers.value(id), it.value())) m_receiptAttempts.insert(it.key(), now);
        }
    }
    emit peersChanged();
    return true;
}

QStringList SyncEngine::peerDescriptions() const {
    QStringList peers;
    for (const auto &p : m_peers) { if (!DeviceAccess::approved(m_profile, p.id, p.publicKey)) continue; peers.append(QStringLiteral("%1 — %2").arg(p.name, p.address.toString())); }
    peers.sort();
    return peers;
}
bool SyncEngine::send(const QJsonObject &payload) {
    QString error;
    if (m_profile.isPaused() || !m_profile.sendingEnabled || !ClipboardPayload::validate(payload, &error)) return false;
    const QByteArray plain = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (plain.size() > SecureProtocol::MaxWireBytes - 40 || m_peers.isEmpty()) return false;
    if (!m_pendingPeers.isEmpty()) { emit message("Transfer queue is busy; try again."); return false; }
    m_pendingPeers.clear();
    for (const auto &peer : m_peers)
        if (DeviceAccess::approved(m_profile, peer.id, peer.publicKey)) m_pendingPeers.append(peer);
    if (m_profile.managed() && !DeviceAccess::approved(m_profile, m_profile.deviceId, DeviceAccess::publicKey(m_profile))) m_pendingPeers.clear();
    if (m_pendingPeers.isEmpty()) return false;
    m_pendingPlain = plain; m_retryPayload = payload;
    while (m_jobs.size() >= 48) {
        auto oldest = m_jobs.end();
        for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it)
            if (it->state != "sending" && (oldest == m_jobs.end() || it->time < oldest->time)) oldest = it;
        if (oldest == m_jobs.end()) break;
        m_jobs.erase(oldest);
    }
    m_batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (const auto &peer : m_pendingPeers) {
        const QString id = m_batchId + "/" + peer.id;
        m_jobs.insert(id, Job{id, peer.id, peer.name, "sending", {}, ClipboardPayload::description(payload), 0, QDateTime::currentMSecsSinceEpoch(), {}});
        m_jobs[id].kind = payload.value("kind").toString();
        m_jobs[id].items = m_jobs[id].kind == "files" ? payload.value("files").toArray().size() : payload.value("text").toString().size();
    }
    emit transfersChanged();
    drainOutgoing();
    emit message("Sending " + ClipboardPayload::description(payload));
    return true;
}
void SyncEngine::drainOutgoing() {
    if (m_profile.isPaused() || m_stopping) return;
    const qint64 reservation = m_pendingPlain.size() + 40;
    while (!m_pendingPeers.isEmpty() && m_connections < 8 && reservation <= TransferMemoryBudget - m_bufferedBytes) {
        const Peer peer = m_pendingPeers.takeFirst();
        const QString id = m_batchId + "/" + peer.id;
        QByteArray outgoing = m_pendingPlain;
        if (peer.receipts) outgoing = QJsonDocument(QJsonObject{{"type", "clipboard"}, {"id", id},
            {"payload", QJsonDocument::fromJson(m_pendingPlain).object()}}).toJson(QJsonDocument::Compact);
        sendTo(peer, outgoing, id);
    }
    if (m_pendingPeers.isEmpty()) m_pendingPlain.clear();
}
bool SyncEngine::sendTo(const Peer &peer, const QByteArray &plain, const QString &jobId) {
    const Profile channel = DeviceAccess::channel(m_profile, peer.id);
    if (!channel.registered()) { if (!jobId.isEmpty()) updateJob(jobId, "failed", 0, "Device access is not approved."); return false; }
    const auto control = jobId.isEmpty() ? QJsonDocument::fromJson(plain).object() : QJsonObject();
    const QString controlId = control.value("type").toString() == "receipt" ? control.value("id").toString() : QString();
    const qint64 reservation = plain.size() + 40;
    if (m_connections >= 8 || reservation > TransferMemoryBudget - m_bufferedBytes) {
        if (!jobId.isEmpty()) updateJob(jobId, "failed", 0, "Transfer queue is busy; try again.");
        emit message("Transfer queue is busy; try again."); return false;
    }
    auto *socket = new QTcpSocket(this);
    if (!jobId.isEmpty()) m_jobs[jobId].socket = socket;
    m_bufferedBytes += reservation;
    ++m_connections;
    connect(socket, &QObject::destroyed, this, [this, reservation] {
        --m_connections; m_bufferedBytes -= reservation;
        if (!m_stopping) QTimer::singleShot(0, this, &SyncEngine::drainOutgoing);
    });
    socket->setReadBufferSize(HelloBytes);
    auto *timeout = deadline(socket);
    if (!jobId.isEmpty()) {
        connect(timeout, &QTimer::timeout, this, [this, jobId, socket] {
            if (m_jobs.value(jobId).socket == socket) updateJob(jobId, "failed", 0, "Timed out. Check Wi-Fi and keep the receiving app available.");
        });
        connect(socket, &QTcpSocket::bytesWritten, this, [this, socket, jobId, size = plain.size()] (qint64) {
            if (m_jobs.value(jobId).state == "sending") updateJob(jobId, "sending", qBound(0, int(100 * (size - qMin(qint64(size), socket->bytesToWrite())) / qMax(qint64(1), qint64(size))), 99));
        });
    }
    const auto sent = std::make_shared<bool>(false);
    const auto receiptContext = std::make_shared<QByteArray>();
    const auto receiptLength = std::make_shared<quint32>(0);
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket, jobId](QAbstractSocket::SocketError) {
        if (!jobId.isEmpty() && m_jobs.value(jobId).socket == socket && m_jobs.value(jobId).state == "sending") updateJob(jobId, "failed", 0, "Transfer failed: " + socket->errorString());
        if (!jobId.isEmpty()) emit message("Transfer failed: " + socket->errorString());
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket, jobId] {
        if (!jobId.isEmpty() && m_jobs.value(jobId).socket == socket && m_jobs.value(jobId).state == "sending") updateJob(jobId, "failed", 0, "Connection closed without delivery confirmation.");
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, timeout, sent, peer, plain, jobId, channel, receiptContext, receiptLength, controlId] {
        if (m_profile.isPaused()) { socket->abort(); return; }
        if (*sent) {
            if (jobId.isEmpty() && controlId.isEmpty()) return;
            if (!*receiptLength) {
                if (socket->bytesAvailable() < 4) return;
                const auto header = socket->read(4);
                *receiptLength = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(header.constData()));
                if (*receiptLength < 40 || *receiptLength > 2048) { socket->abort(); return; }
            }
            if (socket->bytesAvailable() < *receiptLength) return;
            QByteArray plainReceipt;
            if (!SecureProtocol::open(channel, socket->read(*receiptLength), *receiptContext, &plainReceipt)) { socket->abort(); return; }
            const auto receipt = QJsonDocument::fromJson(plainReceipt).object();
            const auto state = receipt.value("state").toString();
            if (!controlId.isEmpty()) {
                if (receipt.value("id").toString() != controlId || state != "receipt-ack") { socket->abort(); return; }
                if (m_receipts.value(controlId).value("state") == receipt.value("receiptState")) {
                    m_receipts.remove(controlId); m_receiptAttempts.remove(controlId);
                }
                timeout->stop(); socket->disconnectFromHost(); return;
            }
            if (receipt.value("id").toString() != jobId || (state != "received" && state != "copied" && state != "failed")) { socket->abort(); return; }
            updateJob(jobId, state, state == "failed" ? 0 : 100, receipt.value("error").toString().left(256));
            timeout->stop(); socket->disconnectFromHost(); return;
        }
        if (socket->bytesAvailable() < HelloBytes) return;
        const QByteArray hello = socket->read(HelloBytes);
        const QByteArray challenge = hello.mid(4, 32), receiver = hello.mid(36, 36);
        if (hello.left(4) != "LC2!" || receiver != peer.id.toUtf8()
            || !SecureProtocol::verify(hello.mid(72, 32), SecureProtocol::mac(m_profile, serverProof(challenge, receiver), "handshake"))) {
            socket->abort(); return;
        }
        *sent = true; timeout->start(30000);
        *receiptContext = context(challenge, m_profile.deviceId.toUtf8(), receiver) + "/receipt";
        socket->setReadBufferSize(2052);
        const QByteArray sender = m_profile.deviceId.toUtf8();
        const QByteArray wire = SecureProtocol::seal(channel, plain, context(challenge, sender, receiver));
        if (wire.isEmpty()) { socket->abort(); return; }
        QByteArray header(4, Qt::Uninitialized);
        qToBigEndian(quint32(wire.size()), reinterpret_cast<uchar *>(header.data()));
        socket->write(sender + SecureProtocol::mac(channel, clientProof(challenge, sender, receiver), "handshake") + header + wire);
        // Wait for an authenticated receipt, never infer delivery from a TCP flush.
        // Deferred Android Copy confirmations use an authenticated control transfer.
        if (!jobId.isEmpty() || !controlId.isEmpty()) timeout->start(30000);
        else socket->disconnectFromHost();
    });
    socket->connectToHost(peer.address, peer.port);
    return true;
}
void SyncEngine::accept() {
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        socket->setParent(this);
        if (m_connections >= 8) { socket->abort(); socket->deleteLater(); continue; }
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
            if (transfer->phase == IncomingTransfer::Done) return;
            if (transfer->phase == IncomingTransfer::Proof) {
                if (socket->bytesAvailable() >= 8 && socket->peek(4) == "LCD?") {
                    const auto header = socket->read(8);
                    const auto length = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(header.constData() + 4));
                    if (!length || length > 32768 || length > TransferMemoryBudget - m_bufferedBytes) { socket->abort(); return; }
                    transfer->length = length; m_bufferedBytes += length;
                    transfer->phase = IncomingTransfer::Discovery;
                    socket->setReadBufferSize(length);
                }
            }
            if (transfer->phase == IncomingTransfer::Discovery) {
                if (socket->bytesAvailable() < transfer->length) return;
                transfer->phase = IncomingTransfer::Done;
                if (processAnnouncement(socket->read(transfer->length), socket->peerAddress(), 45871, socket)) {
                    const auto response = SecureProtocol::announcement(m_profile, port());
                    QByteArray header(4, Qt::Uninitialized);
                    qToBigEndian(quint32(response.size()), reinterpret_cast<uchar *>(header.data()));
                    socket->write(header + response);
                }
                timeout->stop(); socket->disconnectFromHost(); return;
            }
            if (transfer->phase == IncomingTransfer::Proof) {
                if (m_profile.isPaused()) { socket->abort(); return; }
                if (socket->bytesAvailable() < ProofBytes) return;
                const QByteArray proof = socket->read(ProofBytes), sender = proof.left(36);
                const QUuid id(QString::fromUtf8(sender));
                if (id.isNull() || id.toString(QUuid::WithoutBraces).toUtf8() != sender || sender == receiver
                    || !SecureProtocol::verify(proof.mid(36), SecureProtocol::mac(DeviceAccess::channel(m_profile, QString::fromUtf8(sender)), clientProof(challenge, sender, receiver), "handshake"))) {
                    socket->abort(); return;
                }
                transfer->channel = DeviceAccess::channel(m_profile, QString::fromUtf8(sender));
                if (!transfer->channel.registered()) { socket->abort(); return; }
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
            if (!m_profile.isPaused() && SecureProtocol::open(transfer->channel, socket->read(length),
                context(challenge, transfer->sender, receiver), &plain)) {
                auto payload = QJsonDocument::fromJson(plain).object();
                const QString sender = QString::fromUtf8(transfer->sender);
                if (payload.value("type").toString() == "receipt") {
                    const QString id = payload.value("id").toString(), state = payload.value("state").toString();
                    if (m_jobs.contains(id) && m_jobs.value(id).peer == sender && (state == "received" || state == "copied" || state == "failed")) {
                        const auto job = m_jobs.value(id);
                        if (job.state != "cancelled" && job.state != "copied") {
                            updateJob(id, state, state == "failed" ? 0 : 100, payload.value("error").toString().left(256));
                            if (job.socket) job.socket->disconnectFromHost();
                        }
                    }
                    if (!id.isEmpty() && id.size() <= 100 && (state == "received" || state == "copied" || state == "failed")) {
                        const auto acknowledgement = QJsonDocument(QJsonObject{{"id", id}, {"state", "receipt-ack"}, {"receiptState", state}}).toJson(QJsonDocument::Compact);
                        const auto wire = SecureProtocol::seal(transfer->channel, acknowledgement, context(challenge, transfer->sender, receiver) + "/receipt");
                        QByteArray header(4, Qt::Uninitialized); qToBigEndian(quint32(wire.size()), reinterpret_cast<uchar *>(header.data()));
                        socket->write(header + wire);
                    }
                } else {
                    const bool envelope = payload.value("type").toString() == "clipboard";
                    const QString id = envelope ? payload.value("id").toString() : QString();
                    if (envelope) payload = payload.value("payload").toObject();
                    QString error;
                    if (!m_profile.receivingEnabled) error = "Receiving is disabled on this device.";
                    else if (envelope && (id.isEmpty() || id.size() > 100)) error = "Invalid transfer identity.";
                    else if (!ClipboardPayload::validate(payload, &error) && error.isEmpty()) error = "Invalid clipboard payload.";
                    if (!id.isEmpty() && id.size() <= 100) {
                        m_incomingAcks.insert(id, IncomingAck{socket, transfer->channel, context(challenge, transfer->sender, receiver) + "/receipt"});
                        connect(socket, &QObject::destroyed, this, [this, id] {
                            if (m_incomingAcks.value(id).socket.isNull()) m_incomingAcks.remove(id);
                        });
                    }
                    if (!error.isEmpty()) { completeIncoming(id, sender, "failed", error); emit message("Rejected clipboard payload: " + error); }
                    else {
                        if (m_completedIncoming.contains(id) && !id.isEmpty() && m_completedIncoming.value(id) != "failed") completeIncoming(id, sender, m_completedIncoming.value(id));
                        else {
                            m_completedIncoming.remove(id);
                            if (!id.isEmpty()) emit incomingTransfer(id, sender);
                            emit received(payload);
                            if (!id.isEmpty() && !m_completedIncoming.contains(id)) completeIncoming(id, sender, "received");
                            emit message("Received " + ClipboardPayload::description(payload));
                        }
                    }
                }
            }
            socket->disconnectFromHost();
        });
    }
}

void SyncEngine::updateJob(const QString &id, const QString &state, int progress, const QString &error) {
    if (!m_jobs.contains(id)) return;
    auto &job = m_jobs[id];
    if ((job.state == "cancelled" || job.state == "copied") && state != job.state && state != "sending") return;
    job.state = state; job.progress = progress; job.error = error; emit transfersChanged();
}
QVariantList SyncEngine::transfers() const {
    QVariantList result;
    auto jobs = m_jobs.values();
    std::sort(jobs.begin(), jobs.end(), [](const Job &a, const Job &b) { return a.time > b.time || (a.time == b.time && a.id < b.id); });
    for (const auto &job : jobs) result.append(QVariantMap{{"id", job.id}, {"device", job.name}, {"state", job.state},
        {"progress", job.progress}, {"retryable", job.id.startsWith(m_batchId + "/")}, {"error", job.error}, {"description", job.description}, {"kind", job.kind}, {"items", job.items}, {"time", job.time}});
    return result;
}
bool SyncEngine::retry(const QString &id) {
    if (m_profile.isPaused() || !m_profile.sendingEnabled || !m_jobs.contains(id) || !id.startsWith(m_batchId + "/") || m_retryPayload.isEmpty()) return false;
    const auto job = m_jobs.value(id);
    if (job.state == "sending" || !m_peers.contains(job.peer) || !DeviceAccess::approved(m_profile, job.peer)) return false;
    const Peer peer = m_peers.value(job.peer);
    auto payload = m_retryPayload;
    // A retry keeps its identity: receivers do not duplicate already accepted content.
    if (peer.receipts) payload = {{"type", "clipboard"}, {"id", id}, {"payload", payload}};
    updateJob(id, "sending", 0);
    return sendTo(peer, QJsonDocument(payload).toJson(QJsonDocument::Compact), id);
}
void SyncEngine::cancel(const QString &id) {
    if (!m_jobs.contains(id)) return;
    const auto job = m_jobs.value(id);
    updateJob(id, "cancelled", job.progress);
    for (auto it = m_pendingPeers.begin(); it != m_pendingPeers.end();) {
        if (it->id == job.peer) it = m_pendingPeers.erase(it); else ++it;
    }
    if (m_pendingPeers.isEmpty()) m_pendingPlain.clear();
    if (job.socket) job.socket->abort();
}
bool SyncEngine::sendControl(const Peer &peer, const QJsonObject &control) {
    if (!DeviceAccess::approved(m_profile, peer.id, peer.publicKey)) return false;
    return sendTo(peer, QJsonDocument(control).toJson(QJsonDocument::Compact));
}
void SyncEngine::completeIncoming(const QString &id, const QString &sender, const QString &state, const QString &error) {
    if (id.isEmpty() || sender.isEmpty() || id.size() > 100 || (state != "received" && state != "copied" && state != "failed")) return;
    {
        m_completedOrder.removeAll(id); m_completedOrder.append(id);
        m_completedIncoming.insert(id, state);
        while (m_completedOrder.size() > 128) m_completedIncoming.remove(m_completedOrder.takeFirst());
    }
    const QJsonObject control{{"type", "receipt"}, {"id", id}, {"to", sender}, {"state", state}, {"error", error.left(256)}, {"time", double(QDateTime::currentSecsSinceEpoch())}};
    const auto incoming = m_incomingAcks.value(id);
    if (incoming.socket && incoming.socket->state() == QAbstractSocket::ConnectedState) {
        const auto wire = SecureProtocol::seal(incoming.channel, QJsonDocument(control).toJson(QJsonDocument::Compact), incoming.context);
        QByteArray header(4, Qt::Uninitialized); qToBigEndian(quint32(wire.size()), reinterpret_cast<uchar *>(header.data()));
        incoming.socket->write(header + wire); return;
    }
    if (m_receipts.size() >= 128 && !m_receipts.contains(id)) return;
    m_receipts.insert(id, control); m_receiptAttempts.insert(id, QDateTime::currentSecsSinceEpoch());
    if (m_peers.contains(sender)) sendControl(m_peers.value(sender), control);
}
void SyncEngine::loadDevices() {
    QFile file(QDir(Profile::dataDirectory()).filePath("devices.json"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return;
    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.value("group").toString() != m_profile.groupId()) return;
    for (const auto &v : obj.value("devices").toArray()) {
        const auto d = v.toObject();
        if (m_knownDevices.size() < 64 && !QUuid(d.value("id").toString()).isNull()) m_knownDevices.insert(d.value("id").toString(), d);
    }
}
void SyncEngine::saveDevices() {
    QDir().mkpath(Profile::dataDirectory());
    QSaveFile file(QDir(Profile::dataDirectory()).filePath("devices.json"));
    if (!file.open(QIODevice::WriteOnly)) return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QJsonArray list; for (const auto &d : m_knownDevices) list.append(d);
    const auto bytes = QJsonDocument(QJsonObject{{"group", m_profile.groupId()}, {"devices", list}}).toJson(QJsonDocument::Compact);
    if (file.write(bytes) == bytes.size()) file.commit();
}
QVariantList SyncEngine::devices() const {
    QVariantList result;
    for (auto it = m_knownDevices.cbegin(); it != m_knownDevices.cend(); ++it) {
        const auto d = it.value(); const auto id = it.key();
        bool revoked = false;
        for (const auto &v : m_profile.registry.value("devices").toArray()) {
            const auto entry = v.toObject();
            if (entry.value("id").toString() == id) revoked = entry.value("revoked").toBool();
        }
        result.append(QVariantMap{{"id", id}, {"name", d.value("name").toString()}, {"seen", d.value("seen").toDouble()},
            {"fingerprint", DeviceAccess::fingerprint(QByteArray::fromHex(d.value("key").toString().toLatin1()))},
            {"online", m_peers.contains(id)}, {"approved", DeviceAccess::approved(m_profile, id)}, {"revoked", revoked}});
    }
    return result;
}
bool SyncEngine::authorizeDevice(const QString &id, bool revoke) {
    if (!m_knownDevices.contains(id)) return false;
    Profile next = m_profile;
    if (!DeviceAccess::authorize(&next, id, QByteArray::fromHex(m_knownDevices.value(id).value("key").toString().toLatin1()), revoke)) return false;
    Profile latest = Profile::load();
    if (latest.key == next.key && latest.authorityPublic == next.authorityPublic) { latest.registry = next.registry; next = latest; }
    QString error;
    if (!next.save(&error)) { emit message(error); return false; }
    m_profile = next;
    for (auto *socket : findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly)) socket->abort();
    emit registryChanged(); announce(); return true;
}

void SyncEngine::configure(const Profile &profile) {
    if (profile.key != m_profile.key || profile.deviceId != m_profile.deviceId || profile.deviceSecret != m_profile.deviceSecret) return;
    const bool stopSending = !profile.sendingEnabled && m_profile.sendingEnabled;
    const bool registryChanged = profile.registry != m_profile.registry;
    m_profile = profile;
    if (profile.isPaused() || stopSending) {
        m_pendingPlain.clear(); m_pendingPeers.clear();
        const auto ids = m_jobs.keys();
        for (const auto &id : ids) if (m_jobs.value(id).state == "sending") cancel(id);
    }
    if (profile.isPaused() || registryChanged) {
        for (auto *socket : findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly)) socket->abort();
    }
    announce();
}

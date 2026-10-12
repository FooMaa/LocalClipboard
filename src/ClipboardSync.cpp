#include "ClipboardSync.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QNetworkInterface>
#include <QPushButton>
#include <QStatusBar>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QCryptographicHash>
#include <QDataStream>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>

namespace {
constexpr quint16 DiscoveryPort = 45871;
constexpr quint16 TransferPort = 45872;
constexpr int MaxMessageBytes = 1024 * 1024;
constexpr int PeerTimeoutMs = 15000;
}

ClipboardSync::ClipboardSync(QWidget *parent)
    : QMainWindow(parent),
      m_deviceId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_deviceName(QHostInfo::localHostName()),
      m_clipboard(QApplication::clipboard()),
      m_udp(new QUdpSocket(this)),
      m_tcpServer(new QTcpServer(this)),
      m_announceTimer(new QTimer(this)),
      m_peerTimer(new QTimer(this))
{
    setWindowTitle(QStringLiteral("LocalClipboard"));
    resize(520, 420);
    m_clock.start();

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *intro = new QLabel(
        QStringLiteral("LAN-only text clipboard sync. Use only on a trusted private Wi-Fi network."),
        central);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_peersList = new QListWidget(central);
    layout->addWidget(m_peersList, 1);

#ifdef Q_OS_ANDROID
    auto *sendButton = new QPushButton(QStringLiteral("Send current clipboard"), central);
    layout->addWidget(sendButton);
    connect(sendButton, &QPushButton::clicked, this, [this] {
        if (QApplication::applicationState() != Qt::ApplicationActive) return;
        const QString text = m_clipboard->text();
        if (m_pauseCheck->isChecked() || text.isEmpty()) return;
        if (text.toUtf8().size() > MaxMessageBytes) {
            log(QStringLiteral("Clipboard text exceeds 1 MiB; not sent."));
            return;
        }
        const QString hash = currentTextHash(text);
        m_lastText = text;
        m_lastHash = hash;
        m_recentHashes.insert(hash);
        if (m_recentHashes.size() > 128) m_recentHashes.clear();
        broadcastText(text, hash);
        log(QStringLiteral("Sent clipboard text (%1 characters).").arg(text.size()));
    });
    auto *controls = new QVBoxLayout;
#else
    auto *controls = new QHBoxLayout;
#endif
    m_pauseCheck = new QCheckBox(QStringLiteral("Pause sync"), central);
    auto *refreshButton = new QPushButton(QStringLiteral("Refresh discovery"), central);
    auto *aboutButton = new QPushButton(QStringLiteral("Safety / About"), central);
    controls->addWidget(m_pauseCheck);
    controls->addStretch();
    controls->addWidget(refreshButton);
    controls->addWidget(aboutButton);
    layout->addLayout(controls);

    m_statusLabel = new QLabel(QStringLiteral("Starting…"), central);
    layout->addWidget(m_statusLabel);
    setCentralWidget(central);

    connect(refreshButton, &QPushButton::clicked, this, [this] { announce(); });
    connect(aboutButton, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, QStringLiteral("LocalClipboard — safety"),
            QStringLiteral("Text is sent unencrypted to discovered peers on your LAN. "
                           "There is no pairing/authentication. Use only on a trusted private network. "
                           "Do not copy passwords, access tokens, or other secrets while sync is enabled."));
    });

    if (!m_udp->bind(QHostAddress::AnyIPv4, DiscoveryPort,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        log(QStringLiteral("UDP discovery bind failed: %1").arg(m_udp->errorString()));
    }
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, TransferPort)) {
        log(QStringLiteral("TCP server listen failed: %1").arg(m_tcpServer->errorString()));
    }

    connect(m_udp, &QUdpSocket::readyRead, this, &ClipboardSync::readAnnouncement);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &ClipboardSync::onNewConnection);
#ifndef Q_OS_ANDROID
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardSync::onClipboardChanged);
#endif
    connect(m_announceTimer, &QTimer::timeout, this, &ClipboardSync::announce);
    connect(m_peerTimer, &QTimer::timeout, this, &ClipboardSync::prunePeers);

    m_announceTimer->start(3000);
    m_peerTimer->start(3000);
#ifndef Q_OS_ANDROID
    m_lastText = m_clipboard->text();
#endif
    m_lastHash = currentTextHash(m_lastText);
    announce();
    log(QStringLiteral("Device ID: %1").arg(m_deviceId));
    log(QStringLiteral("Listening on UDP %1 and TCP %2").arg(DiscoveryPort).arg(TransferPort));
}

ClipboardSync::~ClipboardSync() = default;

QJsonObject ClipboardSync::baseMessage(const QString &type) const {
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), type);
    obj.insert(QStringLiteral("id"), m_deviceId);
    obj.insert(QStringLiteral("name"), m_deviceName);
    obj.insert(QStringLiteral("port"), static_cast<int>(TransferPort));
    return obj;
}

void ClipboardSync::announce() {
    if (m_quitting) return;
    QJsonObject obj = baseMessage(QStringLiteral("hello"));
    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    // Send to IPv4 broadcast. Some networks block broadcast; manual peer entry is not implemented in this MVP.
    m_udp->writeDatagram(bytes, QHostAddress::Broadcast, DiscoveryPort);
    // Also multicast to the local subnet multicast group for networks that pass it.
    m_udp->writeDatagram(bytes, QHostAddress(QStringLiteral("239.255.45.71")), DiscoveryPort);
    refreshPeers();
}

void ClipboardSync::readAnnouncement() {
    while (m_udp->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udp->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        if (m_udp->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort) < 0)
            continue;

        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(datagram, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) continue;
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("hello")) continue;
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || id == m_deviceId) continue;

        Peer peer;
        peer.id = id;
        peer.name = obj.value(QStringLiteral("name")).toString(QStringLiteral("Unknown device"));
        peer.address = sender;
        peer.port = static_cast<quint16>(obj.value(QStringLiteral("port")).toInt(TransferPort));
        peer.lastSeenMs = m_clock.elapsed();
        m_peers.insert(id, peer);
        refreshPeers();
    }
}

void ClipboardSync::onClipboardChanged() {
    if (m_quitting || m_applyingRemote || m_pauseCheck->isChecked()) return;
    const QString text = m_clipboard->text();
    if (text == m_lastText) return;
    m_lastText = text;
    const QString hash = currentTextHash(text);
    m_lastHash = hash;
    if (text.isEmpty()) return;
    if (text.toUtf8().size() > MaxMessageBytes) {
        log(QStringLiteral("Clipboard text exceeds 1 MiB; not sent."));
        return;
    }
    m_recentHashes.insert(hash);
    if (m_recentHashes.size() > 128) m_recentHashes.clear();
    broadcastText(text, hash);
    log(QStringLiteral("Sent clipboard text (%1 characters).").arg(text.size()));
}

QString ClipboardSync::currentTextHash(const QString &text) const {
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void ClipboardSync::broadcastText(const QString &text, const QString &hash) {
    const auto peers = m_peers.values();
    for (const Peer &peer : peers) sendTextToPeer(peer, text, hash);
}

void ClipboardSync::sendTextToPeer(const Peer &peer, const QString &text, const QString &hash) {
    QJsonObject obj = baseMessage(QStringLiteral("clipboard"));
    obj.insert(QStringLiteral("text"), text);
    obj.insert(QStringLiteral("hash"), hash);
    const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (json.size() > MaxMessageBytes) return;

    auto *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, [socket, json] {
        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<quint32>(json.size());
        frame.append(json);
        socket->write(frame);
        socket->disconnectFromHost();
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        log(QStringLiteral("Could not reach peer: %1").arg(socket->errorString()));
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    socket->connectToHost(peer.address, peer.port);
}

void ClipboardSync::onNewConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &ClipboardSync::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        socket->setProperty("clipboardFrame", QByteArray());
        socket->setProperty("expectedBytes", 0);
    }
}

void ClipboardSync::onClientReadyRead() {
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;
    QByteArray buffer = socket->property("clipboardFrame").toByteArray();
    buffer.append(socket->readAll());

    if (socket->property("expectedBytes").toInt() == 0) {
        if (buffer.size() < 4) {
            socket->setProperty("clipboardFrame", buffer);
            return;
        }
        const auto *p = reinterpret_cast<const uchar *>(buffer.constData());
        const quint32 length = (static_cast<quint32>(p[0]) << 24)
                             | (static_cast<quint32>(p[1]) << 16)
                             | (static_cast<quint32>(p[2]) << 8)
                             | static_cast<quint32>(p[3]);
        if (length == 0 || length > MaxMessageBytes) {
            socket->disconnectFromHost();
            return;
        }
        socket->setProperty("expectedBytes", static_cast<int>(length));
        buffer.remove(0, 4);
    }

    const int expected = socket->property("expectedBytes").toInt();
    if (expected <= 0 || buffer.size() < expected) {
        socket->setProperty("clipboardFrame", buffer);
        return;
    }

    const QByteArray json = buffer.left(expected);
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() == QStringLiteral("clipboard")) {
            const QString sourceId = obj.value(QStringLiteral("id")).toString();
            const QString text = obj.value(QStringLiteral("text")).toString();
            const QString hash = obj.value(QStringLiteral("hash")).toString();
            if (!sourceId.isEmpty() && sourceId != m_deviceId && !text.isEmpty()
                && text.toUtf8().size() <= MaxMessageBytes
                && currentTextHash(text) == hash && !m_recentHashes.contains(hash)) {
                m_recentHashes.insert(hash);
                if (m_recentHashes.size() > 128) m_recentHashes.clear();
                applyRemoteText(text, hash);
            }
        }
    }
    socket->disconnectFromHost();
}

void ClipboardSync::applyRemoteText(const QString &text, const QString &hash) {
    if (m_pauseCheck->isChecked()) return;
#ifdef Q_OS_ANDROID
    if (QApplication::applicationState() != Qt::ApplicationActive) return;
#endif
    m_applyingRemote = true;
    m_lastText = text;
    m_lastHash = hash;
    m_clipboard->setText(text, QClipboard::Clipboard);
    m_applyingRemote = false;
    log(QStringLiteral("Received clipboard text (%1 characters).").arg(text.size()));
}

void ClipboardSync::prunePeers() {
    const qint64 now = m_clock.elapsed();
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        if (now - it->lastSeenMs > PeerTimeoutMs) it = m_peers.erase(it);
        else ++it;
    }
    refreshPeers();
}

void ClipboardSync::refreshPeers() {
    if (!m_peersList) return;
    m_peersList->clear();
    const qint64 now = m_clock.elapsed();
    for (const Peer &peer : m_peers) {
        m_peersList->addItem(QStringLiteral("%1 — %2 (seen %3 s ago)")
            .arg(peer.name, peer.address.toString())
            .arg(qMax<qint64>(0, (now - peer.lastSeenMs) / 1000)));
    }
    m_statusLabel->setText(QStringLiteral("%1 peer(s) · %2")
        .arg(m_peers.size())
        .arg(m_pauseCheck->isChecked() ? QStringLiteral("sync paused") : QStringLiteral("sync active")));
}

void ClipboardSync::log(const QString &message) {
    statusBar()->showMessage(message, 8000);
}

void ClipboardSync::closeEvent(QCloseEvent *event) {
    m_quitting = true;
    QMainWindow::closeEvent(event);
}

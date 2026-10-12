#pragma once

#include <QClipboard>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QMainWindow>
#include <QSet>
#include <QTcpServer>
#include <QUdpSocket>
#include <QTimer>
#include <QUuid>

class QListWidget;
class QLabel;
class QCheckBox;
class QCloseEvent;

class ClipboardSync final : public QMainWindow {
    Q_OBJECT
public:
    explicit ClipboardSync(QWidget *parent = nullptr);
    ~ClipboardSync() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void announce();
    void readAnnouncement();
    void onClipboardChanged();
    void onNewConnection();
    void onClientReadyRead();
    void refreshPeers();

private:
    struct Peer {
        QString id;
        QString name;
        QHostAddress address;
        quint16 port = 45872;
        qint64 lastSeenMs = 0;
    };

    void log(const QString &message);
    void sendTextToPeer(const Peer &peer, const QString &text, const QString &hash);
    void broadcastText(const QString &text, const QString &hash);
    void applyRemoteText(const QString &text, const QString &hash);
    void prunePeers();
    QString currentTextHash(const QString &text) const;
    QJsonObject baseMessage(const QString &type) const;

    QString m_deviceId;
    QString m_deviceName;
    QClipboard *m_clipboard = nullptr;
    QUdpSocket *m_udp = nullptr;
    QTcpServer *m_tcpServer = nullptr;
    QTimer *m_announceTimer = nullptr;
    QTimer *m_peerTimer = nullptr;
    QListWidget *m_peersList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QCheckBox *m_pauseCheck = nullptr;

    QHash<QString, Peer> m_peers;
    QSet<QString> m_recentHashes;
    QString m_lastText;
    QString m_lastHash;
    QString m_pendingRemoteHash;
    bool m_applyingRemote = false;
    bool m_quitting = false;
    QElapsedTimer m_clock;
};

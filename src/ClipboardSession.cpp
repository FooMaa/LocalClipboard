#include "ClipboardSession.h"
#include "Autostart.h"
#include "Async.h"
#include "ClipboardPayload.h"
#include "SyncEngine.h"
#include "DeviceAccess.h"
#include "LocalHistory.h"
#include "ExchangeStore.h"
#include "SourceApplication.h"
#include <QDateTime>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QMimeData>
#include <QMap>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <memory>
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif
#ifdef Q_OS_IOS
#include "IOSBridge.h"
#endif

ClipboardSession::ClipboardSession(const Profile &profile, QObject *parent)
    : QObject(parent), m_profile(profile) {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] { sendClipboard(false); });
#elif defined(Q_OS_ANDROID)
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            if (AndroidBridge::takeCopyRequest()) copyReceivedClipboard();
        }
    });
    auto *poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] {
        if (QApplication::applicationState() != Qt::ApplicationActive) return;
        refresh();
        if (m_profile.registered() && !m_profile.isPaused()) {
            if (AndroidBridge::takeCopyRequest()) copyReceivedClipboard();
            const quint64 revision = m_revision;
            if (m_sharedPayload.isEmpty() && !m_loadingShares) m_loadingShares = AndroidBridge::consumeShares(this, [this, revision](const QJsonObject &p) {
                m_loadingShares = false;
                if (m_revision != revision) return;
                m_sharedPayload = p; emit shareChanged();
            }, [this, revision](const QString &e) { m_loadingShares = false; if (revision == m_revision) report(e); });
        }
    });
    poll->start(1000);
#else
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        // iOS suspends ordinary apps. Tear down sockets so desktops stop listing us.
        if (state == Qt::ApplicationActive) startEngine();
        else { delete m_engine; m_engine = nullptr; refresh(); }
    });
    auto *poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] {
        if (!m_engine || m_profile.isPaused() || m_engine->peerDescriptions().isEmpty()) return;
        QString error;
        const auto urls = IOSBridge::queuedFiles(m_profile);
        if (urls.isEmpty()) return;
        const auto payload = ClipboardPayload::fromFiles(urls, &error);
        if (payload.isEmpty()) { report(error); return; }
        if (sendPayload(payload)) {
            IOSBridge::clearQueue(m_profile);
            report("Sending imported files. Copies remain under Imported files for retry.");
        }
    });
    poll->start(1000);
#endif
    auto *maintenance = new QTimer(this);
    connect(maintenance, &QTimer::timeout, this, [this] {
        if (m_profile.pauseUntil && m_profile.pauseUntil <= QDateTime::currentSecsSinceEpoch()) {
            Profile next = m_profile; next.pauseUntil = 0; saveProfile(next);
        }
        LocalHistory::prune(m_profile);
    });
    maintenance->start(60000);
    startEngine();
}
void ClipboardSession::replaceProfile(const Profile &profile) {
    if (m_profile.key == profile.key && m_profile.deviceId == profile.deviceId && m_profile.deviceSecret == profile.deviceSecret) {
        m_profile = profile;
        LocalHistory::prune(m_profile);
        if (m_engine) m_engine->configure(profile);
        emit profileChanged(); emit stateChanged(); refresh(); return;
    }
    ++m_revision;
    m_profile = profile;
    m_lastHash.clear();
    m_incomingId.clear(); m_incomingSender.clear();
    LocalHistory::prune(m_profile);
    m_sharedPayload = {}; emit shareChanged();
    emit profileChanged();
    startEngine();
}
bool ClipboardSession::saveProfile(const Profile &profile) {
    QString error;
    Profile next = profile;
    const auto latest = Profile::load();
    if (latest.key == next.key && latest.authorityPublic == next.authorityPublic && next.managed())
        DeviceAccess::updateRegistry(&next, latest.registry);
    if (!next.save(&error)) { report(error); return false; }
    replaceProfile(next);
    return true;
}
bool ClipboardSession::setPaused(bool paused) {
    if (m_profile.isPaused() == paused && !m_profile.pauseUntil) return true;
    Profile next = m_profile; next.paused = paused; next.pauseUntil = 0;
    QString error;
    if (!next.save(&error)) { report(error); return false; }
    m_profile = next;
    if (m_engine) m_engine->configure(next);
    emit profileChanged(); emit stateChanged(); refresh();
    return true;
}
bool ClipboardSession::setAutostart(bool enabled) {
    if (m_profile.autostart == enabled) return true;
    QString error;
    if (!Autostart::setEnabled(enabled, &error)) { report(error); return false; }
    Profile next = m_profile; next.autostart = enabled;
    if (!next.save(&error)) {
        Autostart::setEnabled(m_profile.autostart, nullptr);
        report(error); return false;
    }
    m_profile = next; emit profileChanged(); return true;
}
void ClipboardSession::sendFiles(const QList<QUrl> &urls) {
    if (urls.isEmpty() || m_preparingSend || m_profile.isPaused() || !m_profile.sendingEnabled) return;
    setBusy(true); report("Preparing files…");
    const quint64 revision = m_revision;
    Async::run(this, [urls] {
        QString error;
        const auto payload = ClipboardPayload::fromFiles(urls, &error);
        return QPair<QJsonObject, QString>(payload, error);
    }, [this, revision](const QPair<QJsonObject, QString> &result) {
        setBusy(false);
        if (revision != m_revision) return;
        if (!result.first.isEmpty()) sendPayload(result.first); else report(result.second);
    });
}
void ClipboardSession::setBusy(bool busy) {
    if (m_preparingSend == busy) return;
    m_preparingSend = busy; emit busyChanged();
}
QString ClipboardSession::sharedDescription() const {
    return m_sharedPayload.isEmpty() ? QString() : ClipboardPayload::description(m_sharedPayload);
}
void ClipboardSession::finishShare(bool send) {
    const auto payload = m_sharedPayload;
    m_sharedPayload = {}; emit shareChanged();
    if (send && !payload.isEmpty()) sendPayload(payload);
}
bool ClipboardSession::receiving() const {
    if (!m_profile.receivingEnabled || m_profile.isPaused()
        || !DeviceAccess::approved(m_profile, m_profile.deviceId, DeviceAccess::publicKey(m_profile))) return false;
#ifdef Q_OS_ANDROID
    return m_profile.registered();
#else
    return m_engine && m_engine->port();
#endif
}
bool ClipboardSession::pendingClipboard() const {
#ifdef Q_OS_ANDROID
    return AndroidBridge::hasPendingInbox();
#else
    return false;
#endif
}
void ClipboardSession::copyReceivedClipboard() {
    const auto profile = m_profile;
    copyPayload([profile] {
        for (const auto &entry : ExchangeStore::entries(profile)) {
            const auto item = entry.toMap();
            if (!item.value("outgoing").toBool()) return ExchangeStore::item(profile, item.value("id").toString());
        }
        return QJsonObject();
    });
}
bool ClipboardSession::sendText(const QString &text) {
    const QJsonObject payload{{"kind", "text"}, {"text", text}, {"html", ""}};
    QString error;
    if (!ClipboardPayload::validate(payload, &error)) { report(error); return false; }
    return sendPayload(payload);
}
void ClipboardSession::copyExchangeItem(const QString &id) {
    const auto profile = m_profile;
    copyPayload([profile, id] { return ExchangeStore::item(profile, id); });
}
void ClipboardSession::refresh() {
#ifdef Q_OS_ANDROID
    const auto service = AndroidBridge::serviceState(m_profile);
    const auto serviceMessage = service.value("message").toString();
    if (serviceMessage != m_serviceMessage) { m_serviceMessage = serviceMessage; if (!serviceMessage.isEmpty()) report(serviceMessage); }
    const auto devices = service.value("devices").toArray().toVariantList();
    const auto transfers = service.value("transfers").toArray().toVariantList();
    if (devices != m_serviceDevices) { m_serviceDevices = devices; emit stateChanged(); }
    if (transfers != m_serviceTransfers) { m_serviceTransfers = transfers; emit transfersChanged(); }
    Profile latest = Profile::load();
    if (latest.key == m_profile.key && latest.registry != m_profile.registry) { m_profile.registry = latest.registry; emit profileChanged(); }
    const bool pending = pendingClipboard();
    if (pending != m_pendingInbox) { m_pendingInbox = pending; emit stateChanged(); }
    if (m_loadingPeers) return;
    m_loadingPeers = true;
    const Profile profile = m_profile;
    const quint64 revision = m_revision;
    Async::run(this, [profile] { return AndroidBridge::peerDescriptions(profile); }, [this, revision](const QStringList &peers) {
        m_loadingPeers = false;
        if (revision != m_revision || peers == m_peers) return;
        m_peers = peers; emit stateChanged();
    });
#else
    const QStringList peers = m_engine ? m_engine->peerDescriptions() : QStringList();
    if (m_peers == peers) return;
    m_peers = peers; emit stateChanged();
#endif
}
void ClipboardSession::report(const QString &message) {
    if (m_message == message) return;
    m_message = message; emit messageChanged();
}
void ClipboardSession::startEngine() {
    delete m_engine; m_engine = nullptr;
    if (!m_profile.registered()) { refresh(); return; }
#ifdef Q_OS_IOS
    if (QApplication::applicationState() != Qt::ApplicationActive) { refresh(); return; }
#endif
#ifdef Q_OS_ANDROID
    AndroidBridge::startService(m_profile.autostart);
#else
    m_engine = new SyncEngine(m_profile, this);
    connect(m_engine, &SyncEngine::incomingTransfer, this, [this](const QString &id, const QString &sender) { m_incomingId = id; m_incomingSender = sender; });
    connect(m_engine, &SyncEngine::transfersChanged, this, &ClipboardSession::transfersChanged);
    connect(m_engine, &SyncEngine::registryChanged, this, [this] { m_profile.registry = m_engine->profile().registry; emit profileChanged(); emit stateChanged(); });
    connect(m_engine, &SyncEngine::received, this, &ClipboardSession::apply);
    connect(m_engine, &SyncEngine::message, this, &ClipboardSession::report);
    connect(m_engine, &SyncEngine::peersChanged, this, &ClipboardSession::refresh);
#endif
    refresh();
}
void ClipboardSession::sendClipboard(bool explicitSend) {
    if (m_applying || m_profile.isPaused() || !m_profile.sendingEnabled || (!explicitSend && !m_profile.automaticClipboard)) return;
    if (!explicitSend && SourceApplication::supported()) {
        const auto source = SourceApplication::current();
        for (const auto &excluded : m_profile.excludedApplications)
            if (!excluded.isEmpty() && (source.compare(excluded, Qt::CaseInsensitive) == 0 || QFileInfo(source).fileName().compare(excluded, Qt::CaseInsensitive) == 0)) return;
    }
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    if (QApplication::applicationState() != Qt::ApplicationActive) return;
#endif
#ifdef Q_OS_ANDROID
    if (m_preparingSend) return;
    const QMimeData *source = QApplication::clipboard()->mimeData();
    if (!source) return;
    // Access the system clipboard on Qt's GUI thread; encode images/read files on a worker.
    const auto urls = source->urls();
    const QString text = source->text(), html = source->html();
    QImage image;
    if (urls.isEmpty() && source->hasImage()) {
        image = qvariant_cast<QImage>(source->imageData());
        if (image.isNull()) image = qvariant_cast<QPixmap>(source->imageData()).toImage();
    }
    setBusy(true);
    const quint64 revision = m_revision;
    Async::run(this, [urls, text, html, image] {
        QMimeData snapshot;
        if (!urls.isEmpty()) snapshot.setUrls(urls);
        if (!text.isEmpty()) snapshot.setText(text);
        if (!html.isEmpty()) snapshot.setHtml(html);
        if (!image.isNull()) snapshot.setImageData(image);
        QString error;
        const auto payload = ClipboardPayload::capture(&snapshot, &error);
        return QPair<QJsonObject, QString>(payload, error);
    }, [this, revision](const QPair<QJsonObject, QString> &result) {
        setBusy(false);
        if (m_revision != revision) return;
        if (!result.first.isEmpty()) sendPayload(result.first);
        else if (!result.second.isEmpty()) report(result.second);
    });
#else
    QString error;
    const QJsonObject payload = ClipboardPayload::capture(QApplication::clipboard()->mimeData(), &error);
    if (payload.isEmpty()) { m_lastHash.clear(); if (!error.isEmpty()) report(error); return; }
    const QString hash = ClipboardPayload::hash(payload);
    if (!explicitSend && hash == m_lastHash) return;
    m_lastHash = hash;
    sendPayload(payload);
#endif
}
bool ClipboardSession::sendPayload(const QJsonObject &payload) {
    if (m_profile.isPaused() || !m_profile.sendingEnabled) return false;
#ifndef Q_OS_ANDROID
    if (!m_engine || !m_engine->send(payload)) {
        report("No registered device is online, or synchronization is paused."); return false;
    }
#endif
    remember(payload);
    const auto profile = m_profile; const auto revision = m_revision;
#ifdef Q_OS_ANDROID
    const QString description = ClipboardPayload::description(payload);
    AndroidBridge::queueOutgoing(m_profile, payload, this, [this, profile, revision, payload, description](const QString &error) {
        if (revision != m_revision) return;
        report(error.isEmpty() ? "Queued " + description : error);
        if (!error.isEmpty()) return;
        Async::run(this, [profile, payload] { return ExchangeStore::record(profile, payload, {}, {}, true); },
            [this, revision](const QString &) { if (revision == m_revision) emit exchangeChanged(); });
    });
#else
    Async::run(this, [profile, payload] { return ExchangeStore::record(profile, payload, {}, {}, true); },
        [this, revision](const QString &) { if (revision == m_revision) emit exchangeChanged(); });
#endif
    return true;
}
void ClipboardSession::apply(const QJsonObject &payload) {
    if (m_profile.isPaused() || !m_profile.receivingEnabled) { completeReceived("failed", "Receiving is disabled on this device."); return; }
    const auto profile = m_profile;
    const auto id = m_incomingId, sender = m_incomingSender;
    m_incomingId.clear(); m_incomingSender.clear();
    const auto revision = m_revision;
    Async::run(this, [profile, payload, sender, id] { return ExchangeStore::record(profile, payload, sender, id); },
        [this, revision, payload, id, sender](const QString &stored) {
            if (revision != m_revision) return;
            if (m_engine && !id.isEmpty()) m_engine->completeIncoming(id, sender, stored.isEmpty() ? "failed" : "received",
                stored.isEmpty() ? "Could not store received content." : QString());
            if (stored.isEmpty()) { report("Could not store received content."); return; }
            remember(payload); emit exchangeChanged();
            report("New item received. Choose Copy on the home screen.");
        });
}
void ClipboardSession::importFiles(const QList<QUrl> &urls) {
#ifdef Q_OS_IOS
    // IOSApplication already copied the security-scoped URLs during FileOpen.
    QString message;
    if (!IOSBridge::queueFiles(m_profile, urls, &message)) report(message);
    else { report("Imported files queued for your registered devices."); }
#else
    Q_UNUSED(urls);
#endif
}

QVariantList ClipboardSession::devices() const {
#ifdef Q_OS_ANDROID
    return m_serviceDevices;
#else
    return m_engine ? m_engine->devices() : QVariantList();
#endif
}
QVariantList ClipboardSession::transfers() const {
#ifdef Q_OS_ANDROID
    return m_serviceTransfers;
#else
    return m_engine ? m_engine->transfers() : QVariantList();
#endif
}
bool ClipboardSession::authorizeDevice(const QString &id, bool revoke) {
#ifdef Q_OS_ANDROID
    return AndroidBridge::serviceCommand("authorize", id, revoke);
#else
    return m_engine && m_engine->authorizeDevice(id, revoke);
#endif
}
bool ClipboardSession::retryTransfer(const QString &id) {
#ifdef Q_OS_ANDROID
    return AndroidBridge::serviceCommand("retry", id);
#else
    return m_engine && m_engine->retry(id);
#endif
}
void ClipboardSession::cancelTransfer(const QString &id) {
#ifdef Q_OS_ANDROID
    AndroidBridge::serviceCommand("cancel", id);
#else
    if (m_engine) m_engine->cancel(id);
#endif
}
void ClipboardSession::completeReceived(const QString &state, const QString &error) {
    if (m_engine && !m_incomingId.isEmpty()) m_engine->completeIncoming(m_incomingId, m_incomingSender, state, error);
    m_incomingId.clear(); m_incomingSender.clear();
}
void ClipboardSession::remember(const QJsonObject &payload) {
    if (!m_profile.historyEnabled) return;
    const auto profile = m_profile;
    Async::run(this, [profile, payload] { return LocalHistory::record(profile, payload); }, [this](bool ok) {
        if (!ok) report("Could not save local history."); emit historyChanged();
    });
}
void ClipboardSession::copyHistory(const QString &id) {
    if (m_preparingSend || !m_profile.historyEnabled) return;
    const auto profile = m_profile;
    copyPayload([profile, id] { return QJsonObject{{"payload", LocalHistory::payload(profile, id)}}; });
}
void ClipboardSession::copyPayload(const std::function<QJsonObject()> &load) {
    if (m_preparingSend) return;
    struct Copy { QString hash, error, text, html, transferId, sender; QImage image; QList<QUrl> urls; QMap<QString, QByteArray> formats; bool valid = false; };
    const quint64 revision = m_revision;
    setBusy(true);
    Async::run(this, [load] {
        Copy result;
        const auto item = load();
        const auto payload = item.value("payload").toObject();
        result.transferId = item.value("transferId").toString(); result.sender = item.value("sender").toString();
        if (payload.isEmpty()) { result.error = "This item is no longer available."; return result; }
        std::unique_ptr<QMimeData> mime(ClipboardPayload::materialize(payload, QDir(Profile::dataDirectory()).filePath("received"), &result.error));
        if (!mime) return result;
        result.valid = true; result.hash = ClipboardPayload::hash(payload); result.text = mime->text(); result.html = mime->html();
        result.urls = mime->urls(); result.image = qvariant_cast<QImage>(mime->imageData());
        for (const QString &format : {QStringLiteral("x-special/gnome-copied-files"), QStringLiteral("application/x-kde-cutselection"), QStringLiteral("application/x-qt-windows-mime;value=\"Preferred DropEffect\"")})
            if (mime->hasFormat(format)) result.formats.insert(format, mime->data(format));
        return result;
    }, [this, revision](const Copy &result) {
        setBusy(false);
        if (revision != m_revision) return;
        if (!result.valid) { report(result.error); return; }
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        if (QApplication::applicationState() != Qt::ApplicationActive) { report("Open the app to copy this item."); return; }
#endif
        m_applying = true; m_lastHash = result.hash;
#ifdef Q_OS_ANDROID
        if (!result.urls.isEmpty()) AndroidBridge::copyFiles(result.urls);
#elif defined(Q_OS_IOS)
        if (!result.urls.isEmpty()) IOSBridge::copyFiles(result.urls);
#else
        if (!result.urls.isEmpty()) {
            auto *mime = new QMimeData; mime->setUrls(result.urls);
            for (auto it = result.formats.cbegin(); it != result.formats.cend(); ++it) mime->setData(it.key(), it.value());
            QApplication::clipboard()->setMimeData(mime);
        }
#endif
        else {
            auto *mime = new QMimeData;
            if (!result.image.isNull()) mime->setImageData(result.image);
            else { mime->setText(result.text); if (!result.html.isEmpty()) mime->setHtml(result.html); }
            QApplication::clipboard()->setMimeData(mime);
        }
        m_applying = false;
#ifdef Q_OS_ANDROID
        AndroidBridge::acknowledgeTransfer(m_profile, result.transferId, result.sender);
#else
        if (m_engine && !result.transferId.isEmpty()) m_engine->completeIncoming(result.transferId, result.sender, "copied");
#endif
        emit historyChanged(); report("Copied. Paste it into another app.");
    });
}

void ClipboardSession::announce() { if (m_engine) m_engine->announce(); }

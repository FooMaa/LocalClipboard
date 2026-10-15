#include "ClipboardSession.h"
#include "Autostart.h"
#include "Async.h"
#include "ClipboardPayload.h"
#include "SyncEngine.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QMimeData>
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
    QString ignored;
    const auto initial = ClipboardPayload::capture(QApplication::clipboard()->mimeData(), &ignored);
    if (!initial.isEmpty()) m_lastHash = ClipboardPayload::hash(initial);
#elif defined(Q_OS_ANDROID)
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            auto pending = std::move(m_pendingApply); m_pendingApply = {};
            if (pending) pending();
            if (AndroidBridge::takeCopyRequest()) copyReceivedClipboard();
        }
    });
    auto *poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] {
        if (QApplication::applicationState() != Qt::ApplicationActive) return;
        refresh();
        if (m_profile.registered() && !m_profile.paused) {
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
        if (!m_engine || m_profile.paused || m_engine->peerDescriptions().isEmpty()) return;
        QString error;
        const auto urls = IOSBridge::queuedFiles(m_profile);
        if (urls.isEmpty()) return;
        const auto payload = ClipboardPayload::fromFiles(urls, &error);
        if (payload.isEmpty()) { report(error); return; }
        if (m_engine->send(payload)) {
            IOSBridge::clearQueue(m_profile);
            report("Sending imported files. Copies remain under Imported files for retry.");
        }
    });
    poll->start(1000);
#endif
    startEngine();
}
void ClipboardSession::replaceProfile(const Profile &profile) {
    ++m_revision;
    m_profile = profile;
    m_lastHash.clear();
    m_sharedPayload = {}; emit shareChanged();
    if (m_pendingApply) { m_pendingApply = {}; m_applying = false; }
    emit profileChanged();
    startEngine();
}
bool ClipboardSession::saveProfile(const Profile &profile) {
    QString error;
    if (!profile.save(&error)) { report(error); return false; }
    replaceProfile(profile);
    return true;
}
bool ClipboardSession::setPaused(bool paused) {
    if (m_profile.paused == paused) return true;
    Profile next = m_profile; next.paused = paused;
    QString error;
    if (!next.save(&error)) { report(error); return false; }
    m_profile = next;
    if (m_engine) m_engine->setPaused(paused);
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
    if (urls.isEmpty() || m_preparingSend || m_profile.paused) return;
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
#ifdef Q_OS_ANDROID
    if (m_profile.paused) { report("Resume synchronization to copy received content."); return; }
    if (!pendingClipboard()) { report("No received content is waiting."); return; }
    pollAndroidInbox();
#endif
}
void ClipboardSession::refresh() {
#ifdef Q_OS_ANDROID
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
    connect(m_engine, &SyncEngine::received, this, &ClipboardSession::apply);
    connect(m_engine, &SyncEngine::message, this, &ClipboardSession::report);
    connect(m_engine, &SyncEngine::peersChanged, this, &ClipboardSession::refresh);
#endif
    refresh();
}
void ClipboardSession::sendClipboard(bool explicitSend) {
    if (m_applying || m_profile.paused) return;
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
void ClipboardSession::sendPayload(const QJsonObject &payload) {
    if (m_profile.paused) return;
#ifdef Q_OS_ANDROID
    const QString description = ClipboardPayload::description(payload);
    AndroidBridge::queueOutgoing(m_profile, payload, this, [this, description](const QString &error) {
        report(error.isEmpty() ? "Queued " + description : error);
    });
#else
    if (!m_engine || !m_engine->send(payload)) report("No registered device is online, or synchronization is paused.");
#endif
}
void ClipboardSession::apply(const QJsonObject &payload) {
    if (m_profile.paused) return;
#ifdef Q_OS_ANDROID
    struct Prepared { QString hash, description, error, text, html; QList<QUrl> urls; bool valid = false; };
    const QString directory = QDir(Profile::dataDirectory()).filePath("received");
    const quint64 revision = m_revision;
    const QString description = ClipboardPayload::description(payload);
    m_applying = true;
    Async::run(this, [payload, directory, description] {
        Prepared result;
        result.hash = ClipboardPayload::hash(payload);
        result.description = description;
        std::unique_ptr<QMimeData> mime(ClipboardPayload::materialize(payload, directory, &result.error));
        if (mime) {
            result.valid = true; result.urls = mime->urls(); result.text = mime->text(); result.html = mime->html();
        }
        return result;
    }, [this, revision](const Prepared &result) {
        auto finish = [this, revision, result] {
            m_applying = false;
            if (m_revision != revision || m_profile.paused) return;
            if (!result.valid) { AndroidBridge::acknowledgeInbox(); report(result.error); return; }
            m_lastHash = result.hash;
            if (!result.urls.isEmpty()) AndroidBridge::copyFiles(result.urls);
            else {
                auto *mime = new QMimeData;
                mime->setText(result.text); if (!result.html.isEmpty()) mime->setHtml(result.html);
                QApplication::clipboard()->setMimeData(mime);
            }
            AndroidBridge::acknowledgeInbox();
            report("Applied " + result.description);
        };
        if (QApplication::applicationState() == Qt::ApplicationActive) finish();
        else m_pendingApply = std::move(finish);
    });
#else
    const QString hash = ClipboardPayload::hash(payload);
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (hash == m_lastHash) return;
#endif
    QString error;
    QMimeData *mime = ClipboardPayload::materialize(payload, QDir(Profile::dataDirectory()).filePath("received"), &error);
    if (!mime) { report(error); return; }
    m_applying = true;
    m_lastHash = hash;
#ifdef Q_OS_IOS
    if (mime->hasUrls()) { IOSBridge::copyFiles(mime->urls()); delete mime; }
    else QApplication::clipboard()->setMimeData(mime);
#else
    QApplication::clipboard()->setMimeData(mime);
    const auto normalized = ClipboardPayload::capture(QApplication::clipboard()->mimeData(), &error);
    if (!normalized.isEmpty()) m_lastHash = ClipboardPayload::hash(normalized);
#endif
    m_applying = false;
    report("Applied " + ClipboardPayload::description(payload));
#endif
}
void ClipboardSession::pollAndroidInbox() {
#ifdef Q_OS_ANDROID
    if (m_loadingInbox || m_applying || m_profile.paused) return;
    m_loadingInbox = true;
    const quint64 revision = m_revision;
    AndroidBridge::consumeInbox(m_profile, this, [this, revision](const QJsonObject &payload) {
        m_loadingInbox = false;
        if (revision == m_revision && !payload.isEmpty()) apply(payload);
    });
#endif
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

#include "AndroidBridge.h"
#include "Async.h"
#include "ClipboardPayload.h"
#include "I18n.h"
#include "SecureProtocol.h"
#include "SyncEngine.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJniObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QTimer>
#include <QtCore/qnativeinterface.h>

namespace {
constexpr auto Bridge = "org/localclipboard/ClipboardActivity";
QString path(const QString &name) { return QDir(Profile::dataDirectory()).filePath(name); }
QJsonObject read(const QString &name) {
    QFile file(path(name));
    if (!file.open(QIODevice::ReadOnly) || file.size() > SecureProtocol::MaxWireBytes) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool write(const QString &name, const QJsonObject &obj, QString *error = nullptr) {
    QSaveFile file(path(name));
    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(bytes) != bytes.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
void notify() { QJniObject::callStaticMethod<void>("org/localclipboard/ClipboardService", "received", "()V"); }
}
void AndroidBridge::setLanguage(const QString &language) {
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto value = QJniObject::fromString(language);
    QJniObject::callStaticMethod<void>(Bridge, "setLanguage", "(Landroid/content/Context;Ljava/lang/String;)V", context.object(), value.object<jstring>());
}
QString AndroidBridge::installationLanguage() {
    const auto context = QNativeInterface::QAndroidApplication::context();
    return QJniObject::callStaticObjectMethod(Bridge, "installationLanguage", "(Landroid/content/Context;)Ljava/lang/String;", context.object()).toString();
}
bool AndroidBridge::setEnabled(bool enabled) {
    auto context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(Bridge, "setBootEnabled", "(Landroid/content/Context;Z)V", context.object(), jboolean(enabled));
    return true;
}
void AndroidBridge::startService(bool bootEnabled) {
    setEnabled(bootEnabled);
    auto context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(Bridge, "startSync", "(Landroid/content/Context;)V", context.object());
}
void AndroidBridge::stopService() {
    auto context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(Bridge, "stopSync", "(Landroid/content/Context;)V", context.object());
}
void AndroidBridge::queueOutgoing(const Profile &p, const QJsonObject &payload, QObject *owner, const std::function<void(const QString &)> &done) {
    // Serialize writes so a slow older transfer cannot replace a newer outgoing clipboard.
    static QThreadPool pool;
    pool.setMaxThreadCount(1);
    Async::run(owner, [p, payload] {
        QString error;
        if (ClipboardPayload::validate(payload, &error))
            write("outbox.json", QJsonObject{{"group", p.groupId()}, {"time", double(QDateTime::currentSecsSinceEpoch())}, {"payload", payload}}, &error);
        return error;
    }, done, &pool);
}
void AndroidBridge::consumeInbox(const Profile &p, QObject *owner, const std::function<void(const QJsonObject &)> &apply) {
    Async::run(owner, [p] {
        if (p.paused) return QJsonObject();
        // Rename first so a concurrent delivery cannot be deleted with the previous item.
        const QString claimed = "inbox-claimed.json";
        // Keep a claimed item until applied, including across Activity/process restarts.
        if (!QFile::exists(path(claimed)) && !QFile::rename(path("inbox.json"), path(claimed))) return QJsonObject();
        const QJsonObject obj = read(claimed);
        if (obj.value("group").toString() == p.groupId() && !obj.value("payload").toObject().isEmpty())
            return obj.value("payload").toObject();
        QFile::remove(path(claimed));
        return QJsonObject();
    }, apply);
}
bool AndroidBridge::hasPendingInbox() { return QFile::exists(path("inbox.json")) || QFile::exists(path("inbox-claimed.json")); }
bool AndroidBridge::takeCopyRequest() { return QJniObject::callStaticMethod<jboolean>(Bridge, "takeCopyRequest", "()Z"); }
void AndroidBridge::notificationSettings() {
    QJniObject::callStaticMethod<void>(Bridge, "notificationSettings", "(Landroid/content/Context;)V", QNativeInterface::QAndroidApplication::context().object());
}
void AndroidBridge::acknowledgeInbox() {
    QFile::remove(path("inbox-claimed.json"));
    if (!hasPendingInbox()) QJniObject::callStaticMethod<void>(Bridge, "clearReceivedNotification", "(Landroid/content/Context;)V", QNativeInterface::QAndroidApplication::context().object());
}
bool AndroidBridge::consumeShares(QObject *owner, const std::function<void(const QJsonObject &)> &send, const std::function<void(const QString &)> &error) {
    const auto shared = QJniObject::callStaticObjectMethod(Bridge, "takeShare", "()Ljava/lang/String;");
    if (!shared.isValid() || shared.toString().isEmpty()) return false;
    const QByteArray bytes = shared.toString().toUtf8();
    Async::run(owner, [bytes] {
        const QJsonObject obj = QJsonDocument::fromJson(bytes).object();
        if (obj.contains("error")) return obj;
        if (obj.contains("files")) {
            QList<QUrl> urls;
            for (const auto &item : obj.value("files").toArray()) urls.append(QUrl::fromLocalFile(item.toString()));
            QString message;
            const auto payload = ClipboardPayload::fromFiles(urls, &message);
            for (const auto &url : urls) {
                QFile::remove(url.toLocalFile());
                // Only remove an empty staging directory; never recurse over shared input paths.
                QDir().rmdir(QFileInfo(url.toLocalFile()).absolutePath());
            }
            return QJsonObject{{"payload", payload}, {"error", message}};
        }
        const QJsonObject payload{{"kind", "text"}, {"text", obj.value("text")}, {"html", ""}};
        QString message;
        if (!ClipboardPayload::validate(payload, &message)) return QJsonObject{{"error", message}};
        return QJsonObject{{"payload", payload}};
    }, [send, error](const QJsonObject &result) {
        if (!result.value("error").toString().isEmpty()) error(result.value("error").toString());
        else send(result.value("payload").toObject());
    });
    return true;
}
QStringList AndroidBridge::peerDescriptions(const Profile &p) {
    const QJsonObject obj = read("service-state.json");
    if (obj.value("group").toString() != p.groupId()
        || QDateTime::currentSecsSinceEpoch() - qint64(obj.value("time").toDouble()) > 10) return {};
    QStringList peers;
    for (const auto &peer : obj.value("peers").toArray()) peers.append(peer.toString());
    return peers;
}
void AndroidBridge::copyFiles(const QList<QUrl> &urls) {
    QJsonArray paths;
    for (const auto &url : urls) paths.append(url.toLocalFile());
    const QJniObject json = QJniObject::fromString(QString::fromUtf8(QJsonDocument(paths).toJson(QJsonDocument::Compact)));
    auto context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(Bridge, "copyFiles", "(Landroid/content/Context;Ljava/lang/String;)V", context.object(), json.object<jstring>());
}
void AndroidBridge::openReceivedFiles(QWidget *parent) {
    // Android has no portable folder-opening intent. The file chooser lists the private received files.
    const QString chosen = QFileDialog::getOpenFileName(parent, I18n::text("Received files"), path("received"));
    if (chosen.isEmpty()) return;
    openFile(chosen);
}
void AndroidBridge::openFile(const QString &chosen) {
    auto context = QNativeInterface::QAndroidApplication::context();
    const QJniObject file = QJniObject::fromString(chosen);
    QJniObject::callStaticMethod<void>(Bridge, "openFile", "(Landroid/content/Context;Ljava/lang/String;)V", context.object(), file.object<jstring>());
}
int AndroidBridge::runService(QCoreApplication &app) {
    QDir().mkpath(Profile::dataDirectory());
    QLockFile lock(path("service.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) return 0;
    Profile profile = Profile::load();
    if (!profile.registered()) return 0;
    SyncEngine *engine = nullptr;
    auto start = [&] {
        delete engine;
        engine = new SyncEngine(profile, &app);
        QObject::connect(engine, &SyncEngine::received, &app, [&](const QJsonObject &payload) {
            if (write("inbox.json", QJsonObject{{"group", profile.groupId()}, {"payload", payload}})) notify();
        });
    };
    start();
    QTimer poll;
    qint64 lastStateWrite = 0;
    QStringList lastPeers;
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        const Profile latest = Profile::load();
        if (!latest.registered()) return;
        if (latest.key != profile.key || latest.deviceName != profile.deviceName || latest.peerAddresses != profile.peerAddresses) {
            profile = latest; start();
        } else { profile = latest; engine->setPaused(profile.paused); }
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const QStringList descriptions = engine->peerDescriptions();
        // Offline polling must not parse a potentially 48 MiB document every second.
        const QFileInfo queued(path("outbox.json"));
        if (queued.exists() && queued.lastModified().secsTo(QDateTime::currentDateTime()) > 600)
            QFile::remove(queued.absoluteFilePath());
        if (!profile.paused && !descriptions.isEmpty()) {
            const QString claimed = "outbox-claimed.json";
            if (QFile::exists(path(claimed)) || QFile::rename(path("outbox.json"), path(claimed))) {
                const QJsonObject outgoing = read(claimed);
                const bool expired = now - qint64(outgoing.value("time").toDouble()) > 600;
                if (outgoing.value("group").toString() != profile.groupId() || expired
                    || engine->send(outgoing.value("payload").toObject())) QFile::remove(path(claimed));
                // A busy engine retains its claimed item for the next tick, including across restarts.
            }
        }
        if (now - lastStateWrite >= 3 || descriptions != lastPeers) {
            write("service-state.json", QJsonObject{{"group", profile.groupId()}, {"time", double(now)}, {"peers", QJsonArray::fromStringList(descriptions)}});
            lastStateWrite = now; lastPeers = descriptions;
        }
    });
    poll.start(1000);
    const int result = app.exec();
    delete engine;
    return result;
}

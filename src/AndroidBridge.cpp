#include "AndroidBridge.h"
#include "ClipboardPayload.h"
#include "I18n.h"
#include "SecureProtocol.h"
#include "SyncEngine.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
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
bool AndroidBridge::queueOutgoing(const Profile &p, const QJsonObject &payload, QString *error) {
    if (!ClipboardPayload::validate(payload, error)) return false;
    return write("outbox.json", QJsonObject{{"group", p.groupId()}, {"time", double(QDateTime::currentSecsSinceEpoch())}, {"payload", payload}}, error);
}
void AndroidBridge::consumeInbox(const Profile &p, const std::function<void(const QJsonObject &)> &apply) {
    if (p.paused) return;
    // Rename first so a concurrent service delivery cannot be deleted after applying an older item.
    const QString claimed = "inbox-claimed.json";
    QFile::remove(path(claimed));
    if (!QFile::rename(path("inbox.json"), path(claimed))) return;
    const QJsonObject obj = read(claimed);
    if (obj.value("group").toString() == p.groupId()) apply(obj.value("payload").toObject());
    QFile::remove(path(claimed));
}
void AndroidBridge::consumeShares(const std::function<void(const QJsonObject &)> &send, const std::function<void(const QString &)> &error) {
    const auto shared = QJniObject::callStaticObjectMethod(Bridge, "takeShare", "()Ljava/lang/String;");
    if (!shared.isValid() || shared.toString().isEmpty()) return;
    const QJsonObject obj = QJsonDocument::fromJson(shared.toString().toUtf8()).object();
    if (obj.contains("error")) { error(obj.value("error").toString()); return; }
    if (obj.contains("files")) {
        QList<QUrl> urls;
        for (const auto &item : obj.value("files").toArray()) urls.append(QUrl::fromLocalFile(item.toString()));
        QString message;
        const auto payload = ClipboardPayload::fromFiles(urls, &message);
        if (!payload.isEmpty()) send(payload); else error(message);
        for (const auto &url : urls) QFile::remove(url.toLocalFile());
    } else {
        const QJsonObject payload{{"kind", "text"}, {"text", obj.value("text")}, {"html", ""}};
        QString message;
        if (ClipboardPayload::validate(payload, &message)) send(payload); else error(message);
    }
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
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        const Profile latest = Profile::load();
        if (!latest.registered()) return;
        if (latest.key != profile.key || latest.deviceName != profile.deviceName || latest.peerAddresses != profile.peerAddresses) {
            profile = latest; start();
        } else { profile = latest; engine->setPaused(profile.paused); }
        const QJsonObject outgoing = read("outbox.json");
        if (!outgoing.isEmpty()) {
            if (outgoing.value("group").toString() != profile.groupId()
                || QDateTime::currentSecsSinceEpoch() - qint64(outgoing.value("time").toDouble()) > 600)
                QFile::remove(path("outbox.json"));
            else if (!profile.paused && !engine->peerDescriptions().isEmpty()) {
                // Claim the file atomically; the activity can queue another clipboard during transmission.
                QFile::remove(path("outbox-claimed.json"));
                if (QFile::rename(path("outbox.json"), path("outbox-claimed.json"))) {
                    engine->send(read("outbox-claimed.json").value("payload").toObject());
                    QFile::remove(path("outbox-claimed.json"));
                }
            }
        }
        QJsonArray peers;
        for (const auto &peer : engine->peerDescriptions()) peers.append(peer);
        write("service-state.json", QJsonObject{{"group", profile.groupId()}, {"time", double(QDateTime::currentSecsSinceEpoch())}, {"peers", peers}});
    });
    poll.start(1000);
    const int result = app.exec();
    delete engine;
    return result;
}

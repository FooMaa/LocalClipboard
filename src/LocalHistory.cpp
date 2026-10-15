#include "LocalHistory.h"
#include "ClipboardPayload.h"
#include "SecureProtocol.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QRecursiveMutex>
#include <QMutexLocker>

namespace {
QRecursiveMutex historyMutex;
QString root() { return QDir(Profile::dataDirectory()).filePath("history"); }
Profile storageProfile(Profile p) {
    // Device-private history remains private even to another member with the pairing code.
    if (p.managed()) p.key = p.deviceSecret;
    return p;
}
QJsonObject read(const Profile &p, const QString &id) {
    static const QRegularExpression valid("^[0-9]{13}-[0-9a-f-]{36}$");
    if (!p.historyEnabled || !valid.match(id).hasMatch()) return {};
    const QFileInfo info(QDir(root()).filePath(id));
    if (info.isSymLink() || !info.isFile() || info.size() > qint64(p.historyMiB) * 1024 * 1024) return {};
    QFile file(info.filePath()); if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray plain;
    if (!SecureProtocol::open(storageProfile(p), file.readAll(), "LocalClipboard/history/v1/" + id.toUtf8(), &plain)) return {};
    const auto obj = QJsonDocument::fromJson(plain).object(); QString error;
    if (!ClipboardPayload::validate(obj.value("payload").toObject(), &error)) return {};
    return obj;
}
}
void LocalHistory::clear() {
    const QMutexLocker lock(&historyMutex);
    QDir dir(root());
    // Never follow a substituted history directory outside the app data directory.
    if (QFileInfo(root()).isSymLink()) { QFile::remove(root()); return; }
    for (const auto &info : dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System)) QFile::remove(info.filePath());
    QDir().rmdir(root());
}
void LocalHistory::prune(const Profile &p) {
    const QMutexLocker lock(&historyMutex);
    if (!p.historyEnabled) { clear(); return; }
    if (QFileInfo(root()).isSymLink()) return;
    qint64 bytes = 0; int count = 0;
    const auto cutoff = QDateTime::currentMSecsSinceEpoch() - qint64(qBound(1, p.historyDays, 30)) * 86400000;
    for (const auto &file : QDir(root()).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name | QDir::Reversed)) {
        bool valid = false; const auto timestamp = file.fileName().left(13).toLongLong(&valid);
        bytes += file.size(); ++count;
        if (!valid || timestamp < cutoff || bytes > qint64(qBound(1, p.historyMiB, 64)) * 1024 * 1024 || count > 100) QFile::remove(file.filePath());
    }
}
bool LocalHistory::record(const Profile &p, const QJsonObject &payload, QString *error) {
    const QMutexLocker lock(&historyMutex);
    if (!p.historyEnabled) return true;
    const Profile latest = Profile::load();
    if (!latest.historyEnabled || latest.key != p.key || latest.deviceId != p.deviceId) return true;
    QString validation;
    if (!ClipboardPayload::validate(payload, &validation)) return false;
    const auto plain = QJsonDocument(QJsonObject{{"payload", payload}}).toJson(QJsonDocument::Compact);
    if (plain.size() + 40 > qint64(qBound(1, p.historyMiB, 64)) * 1024 * 1024) return true;
    if (QFileInfo(root()).isSymLink() || !QDir().mkpath(root())) return false;
    QFile::setPermissions(root(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto wire = SecureProtocol::seal(storageProfile(p), plain, "LocalClipboard/history/v1/" + id.toUtf8());
    QSaveFile file(QDir(root()).filePath(id));
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(wire) != wire.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    prune(p); return true;
}
QVariantList LocalHistory::entries(const Profile &p, const QString &query) {
    const QMutexLocker lock(&historyMutex);
    prune(p); QVariantList result;
    if (!p.historyEnabled || QFileInfo(root()).isSymLink()) return result;
    for (const auto &file : QDir(root()).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name | QDir::Reversed)) {
        const auto obj = read(p, file.fileName()); if (obj.isEmpty()) continue;
        const auto payload = obj.value("payload").toObject();
        QString preview = payload.value("kind").toString() == "text" ? payload.value("text").toString() : QStringLiteral("image");
        if (payload.value("kind").toString() == "files") {
            QStringList names;
            for (const auto &file : payload.value("files").toArray()) names.append(file.toObject().value("name").toString());
            preview = names.join(", ");
        }
        if (!query.isEmpty() && !preview.contains(query, Qt::CaseInsensitive)) continue;
        result.append(QVariantMap{{"id", file.fileName()}, {"text", preview.left(240)}, {"time", file.fileName().left(13).toLongLong()}, {"kind", payload.value("kind").toString()}});
    }
    return result;
}
QJsonObject LocalHistory::payload(const Profile &p, const QString &id) { const QMutexLocker lock(&historyMutex); prune(p); return read(p, id).value("payload").toObject(); }

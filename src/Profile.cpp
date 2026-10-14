#include "Profile.h"
#include "SecureProtocol.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

bool Profile::registered() const {
    return key.size() == 32 && !QUuid(deviceId).isNull()
        && !deviceName.trimmed().isEmpty() && !ownerName.trimmed().isEmpty();
}
QString Profile::groupId() const {
    return QString::fromLatin1(QCryptographicHash::hash(
        QByteArray("LocalClipboard/profile/v2") + key, QCryptographicHash::Sha256).toHex());
}
QString Profile::invitation() const { return QStringLiteral("LC2.") + QString::fromLatin1(key.toHex()); }
bool Profile::join(const QString &code) {
    const QString clean = code.trimmed();
    if (!QRegularExpression(QStringLiteral("^LC2\\.[0-9a-fA-F]{64}$")).match(clean).hasMatch()) return false;
    key = QByteArray::fromHex(clean.mid(4).toLatin1());
    return true;
}
void Profile::create() { key = SecureProtocol::randomBytes(32); }
QString Profile::dataDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
bool Profile::save(QString *error, const QString &directory) const {
    QDir dir(directory.isEmpty() ? dataDirectory() : directory);
    if (!dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Cannot create profile directory.");
        return false;
    }
    QFile::setPermissions(dir.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QSaveFile file(dir.filePath(QStringLiteral("profile.json")));
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {"version", 2}, {"deviceId", deviceId}, {"deviceName", deviceName.left(80)},
        {"ownerName", ownerName.left(80)}, {"key", QString::fromLatin1(key.toHex())},
        {"autostart", autostart}, {"paused", paused}
    }).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
Profile Profile::load(const QString &directory) {
    Profile p;
    QFile file(QDir(directory.isEmpty() ? dataDirectory() : directory).filePath(QStringLiteral("profile.json")));
    if (file.open(QIODevice::ReadOnly) && file.size() <= 8192) {
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        if (obj.value("version").toInt() == 2) {
            p.deviceId = obj.value("deviceId").toString();
            p.deviceName = obj.value("deviceName").toString().left(80);
            p.ownerName = obj.value("ownerName").toString().left(80);
            p.join(QStringLiteral("LC2.") + obj.value("key").toString());
            p.autostart = obj.value("autostart").toBool(true);
            p.paused = obj.value("paused").toBool(false);
        }
    }
    if (QUuid(p.deviceId).isNull()) p.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (p.deviceName.isEmpty()) p.deviceName = QHostInfo::localHostName().left(80);
    return p;
}

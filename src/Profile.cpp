#include "Profile.h"
#include "SecureProtocol.h"
#include "SecretStore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

bool Profile::registered() const {
    const auto validName = [](const QString &name) {
        static const QRegularExpression forbidden("[\\x00-\\x1f\\x7f\\x{202a}-\\x{202e}\\x{2066}-\\x{2069}]");
        return !name.trimmed().isEmpty() && name.size() <= 80
            && !forbidden.match(name).hasMatch();
    };
    return key.size() == 32 && !QUuid(deviceId).isNull()
        && QUuid(deviceId).toString(QUuid::WithoutBraces) == deviceId
        && validName(deviceName) && validName(ownerName);
}
bool Profile::parseAddresses(const QString &value, QStringList *addresses) {
    if (!addresses || value.size() > 1024) return false;
    QStringList parsed;
    for (const auto &entry : value.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts)) {
        const QHostAddress address(entry);
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isNull()
            || address.isMulticast() || address == QHostAddress::Broadcast) return false;
        if (!parsed.contains(address.toString())) parsed.append(address.toString());
        if (parsed.size() > 32) return false;
    }
    *addresses = parsed;
    return true;
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
    QStringList addresses;
    if (!registered() || !parseAddresses(peerAddresses.join(' '), &addresses)) {
        if (error) *error = QStringLiteral("Invalid device profile.");
        return false;
    }
    const QString storedKey = SecretStore::encode(key);
    if (storedKey.isEmpty()) { if (error) *error = QStringLiteral("Cannot protect profile key."); return false; }
    QDir dir(directory.isEmpty() ? dataDirectory() : directory);
    if (!dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Cannot create profile directory.");
        return false;
    }
    if (!QFile::setPermissions(dir.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        if (error) *error = QStringLiteral("Cannot protect profile directory.");
        return false;
    }
    QSaveFile file(dir.filePath(QStringLiteral("profile.json")));
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        if (error) *error = QStringLiteral("Cannot protect profile file.");
        return false;
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {"version", 2}, {"deviceId", deviceId}, {"deviceName", deviceName.left(80)},
        {"ownerName", ownerName.left(80)}, {"key", storedKey},
        {"autostart", autostart}, {"paused", paused},
        {"peerAddresses", QJsonArray::fromStringList(addresses)}
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
            p.key = SecretStore::decode(obj.value("key").toString());
            p.autostart = obj.value("autostart").toBool(true);
            p.paused = obj.value("paused").toBool(false);
            for (const auto &entry : obj.value("peerAddresses").toArray()) {
                const QHostAddress address(entry.toString());
                if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isNull()
                    && !address.isMulticast() && address != QHostAddress::Broadcast && p.peerAddresses.size() < 32)
                    p.peerAddresses.append(address.toString());
            }
        }
    }
    if (QUuid(p.deviceId).isNull()) p.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (p.deviceName.isEmpty()) p.deviceName = QHostInfo::localHostName().left(80);
    return p;
}

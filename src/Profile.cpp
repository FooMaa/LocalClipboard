#include "Profile.h"
#include "SecureProtocol.h"
#include "SecretStore.h"
#include "DeviceAccess.h"
#include <QDateTime>
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
bool Profile::isPaused() const { return paused || pauseUntil > QDateTime::currentSecsSinceEpoch(); }
void Profile::createManaged() { create(); DeviceAccess::initialize(this); }
QString Profile::invitation() const {
    if (!managed()) return QStringLiteral("LC2.") + QString::fromLatin1(key.toHex());
    return "LC3." + QString::fromLatin1(QJsonDocument(QJsonObject{{"key", QString::fromLatin1(key.toHex())},
        {"authority", QString::fromLatin1(authorityPublic.toHex())}}).toJson(QJsonDocument::Compact)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
bool Profile::join(const QString &code) {
    const QString clean = code.trimmed();
    if (clean.startsWith("LC3.")) {
        if (clean.size() > 512) return false;
        const auto decoded = QByteArray::fromBase64Encoding(clean.mid(4).toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) return false;
        const auto obj = QJsonDocument::fromJson(decoded.decoded).object();
        const auto hexKey = obj.value("key").toString(), authority = obj.value("authority").toString();
        const QRegularExpression hex("^[0-9a-f]{64}$");
        if (obj.size() != 2 || !hex.match(hexKey).hasMatch() || !hex.match(authority).hasMatch()) return false;
        key = QByteArray::fromHex(hexKey.toLatin1()); authorityPublic = QByteArray::fromHex(authority.toLatin1());
        authoritySeed.clear(); registry = {}; deviceSecret = SecureProtocol::randomBytes(32);
        deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        return true;
    }
    if (!QRegularExpression(QStringLiteral("^LC2\\.[0-9a-fA-F]{64}$")).match(clean).hasMatch()) return false;
    key = QByteArray::fromHex(clean.mid(4).toLatin1());
    deviceSecret.clear(); authoritySeed.clear(); authorityPublic.clear(); registry = {};
    return true;
}
void Profile::create() { key = SecureProtocol::randomBytes(32); deviceSecret.clear(); authoritySeed.clear(); authorityPublic.clear(); registry = {}; }
QString Profile::dataDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
bool Profile::save(QString *error, const QString &directory) const {
    QStringList addresses;
    if ((managed() && !registry.isEmpty() && !DeviceAccess::validRegistry(*this, registry))
        || (!authorityPublic.isEmpty() && !managed()) || !registered() || !parseAddresses(peerAddresses.join(' '), &addresses)) {
        if (error) *error = QStringLiteral("Invalid device profile.");
        return false;
    }
    const QString storedKey = SecretStore::encode(key);
    if (storedKey.isEmpty()) { if (error) *error = QStringLiteral("Cannot protect profile key."); return false; }
    const QString storedDevice = deviceSecret.isEmpty() ? QString() : SecretStore::encode(deviceSecret);
    const QString storedAuthority = authoritySeed.isEmpty() ? QString() : SecretStore::encode(authoritySeed);
    if ((!deviceSecret.isEmpty() && storedDevice.isEmpty()) || (!authoritySeed.isEmpty() && storedAuthority.isEmpty())) {
        if (error) *error = QStringLiteral("Cannot protect profile key."); return false;
    }
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
        {"version", managed() ? 3 : 2}, {"deviceId", deviceId}, {"deviceName", deviceName.left(80)},
        {"ownerName", ownerName.left(80)}, {"key", storedKey},
        {"autostart", autostart}, {"paused", paused}, {"pauseUntil", double(pauseUntil)},
        {"sendingEnabled", sendingEnabled}, {"receivingEnabled", receivingEnabled}, {"historyEnabled", historyEnabled},
        {"historyDays", historyDays}, {"historyMiB", historyMiB}, {"excludedApplications", QJsonArray::fromStringList(excludedApplications)},
        {"deviceSecret", storedDevice},
        {"authoritySeed", storedAuthority},
        {"authorityPublic", QString::fromLatin1(authorityPublic.toHex())}, {"registry", registry},
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
    if (file.open(QIODevice::ReadOnly) && file.size() <= 65536) {
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        if (obj.value("version").toInt() == 2 || obj.value("version").toInt() == 3) {
            p.deviceId = obj.value("deviceId").toString();
            p.deviceName = obj.value("deviceName").toString().left(80);
            p.ownerName = obj.value("ownerName").toString().left(80);
            p.key = SecretStore::decode(obj.value("key").toString());
            p.autostart = obj.value("autostart").toBool(true);
            p.paused = obj.value("paused").toBool(false);
            p.pauseUntil = qint64(qBound(0.0, obj.value("pauseUntil").toDouble(), 4102444800.0));
            p.sendingEnabled = obj.value("sendingEnabled").toBool(true); p.receivingEnabled = obj.value("receivingEnabled").toBool(true);
            p.historyEnabled = obj.value("historyEnabled").toBool(false);
            p.historyDays = qBound(1, obj.value("historyDays").toInt(7), 30); p.historyMiB = qBound(1, obj.value("historyMiB").toInt(16), 64);
            p.deviceSecret = SecretStore::decode(obj.value("deviceSecret").toString());
            p.authoritySeed = SecretStore::decode(obj.value("authoritySeed").toString());
            p.authorityPublic = QByteArray::fromHex(obj.value("authorityPublic").toString().toLatin1());
            p.registry = obj.value("registry").toObject();
            if ((obj.value("version").toInt() == 3 && !p.managed())
                || (!obj.value("authoritySeed").toString().isEmpty() && p.authoritySeed.size() != 32)
                || (!p.authorityPublic.isEmpty() && !p.managed()) || (!p.registry.isEmpty() && !DeviceAccess::validRegistry(p, p.registry))) p.key.clear();
            for (const auto &v : obj.value("excludedApplications").toArray())
                if (v.isString() && p.excludedApplications.size() < 64) p.excludedApplications.append(v.toString().left(256));
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

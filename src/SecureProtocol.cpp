#include "SecureProtocol.h"
#include "monocypher.h"
#include "DeviceAccess.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QtEndian>
#include <QUuid>
#include <cmath>

namespace {
const uint8_t *ptr(const QByteArray &v) { return reinterpret_cast<const uint8_t *>(v.constData()); }
uint8_t *ptr(QByteArray &v) { return reinterpret_cast<uint8_t *>(v.data()); }
QByteArray subkey(const Profile &p, const QByteArray &domain) {
    QByteArray result(32, Qt::Uninitialized);
    const QByteArray label = QByteArray("LocalClipboard/v2/") + domain;
    crypto_blake2b_keyed(ptr(result), 32, ptr(p.key), 32, ptr(label), size_t(label.size()));
    return result;
}
}
QByteArray SecureProtocol::randomBytes(int size) {
    QByteArray result(size, Qt::Uninitialized);
    for (int i = 0; i < size; i += 4) {
        const quint32 word = QRandomGenerator::system()->generate();
        const int count = qMin(4, size - i);
        memcpy(result.data() + i, &word, size_t(count));
    }
    return result;
}
QByteArray SecureProtocol::mac(const Profile &p, const QByteArray &message, const QByteArray &domain) {
    if (!p.registered()) return {};
    QByteArray key = subkey(p, domain);
    QByteArray result(32, Qt::Uninitialized);
    crypto_blake2b_keyed(ptr(result), 32, ptr(key), 32, ptr(message), size_t(message.size()));
    crypto_wipe(key.data(), size_t(key.size()));
    return result;
}
bool SecureProtocol::verify(const QByteArray &a, const QByteArray &b) {
    return a.size() == 32 && b.size() == 32 && crypto_verify32(ptr(a), ptr(b)) == 0;
}
QByteArray SecureProtocol::announcement(const Profile &p, quint16 port) {
    QJsonObject obj{{"version", p.managed() ? 3 : 2}, {"group", p.groupId()}, {"id", p.deviceId},
                    {"name", p.deviceName}, {"port", port}, {"time", double(QDateTime::currentSecsSinceEpoch())}};
    obj.insert("receipts", true);
    if (p.managed()) { obj.insert("publicKey", QString::fromLatin1(DeviceAccess::publicKey(p).toHex())); obj.insert("registry", p.registry); }
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    obj.insert("mac", QString::fromLatin1(mac(p, body, "discovery").toHex()));
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}
bool SecureProtocol::readAnnouncement(const Profile &p, const QByteArray &bytes, QJsonObject *object) {
    if (!object || bytes.size() > 32768 || !p.registered()) return false;
    QJsonObject obj = QJsonDocument::fromJson(bytes).object();
    const auto encodedMac = obj.take("mac").toString().toLatin1();
    const QByteArray tag = QByteArray::fromHex(encodedMac);
    if ((obj.size() != (p.managed() ? 9 : 7) && (p.managed() || obj.size() != 6)) || encodedMac.size() != 64 || tag.toHex() != encodedMac
        || obj.value("version").toInt() != (p.managed() ? 3 : 2) || obj.value("group").toString() != p.groupId()
        || QUuid(obj.value("id").toString()).isNull()
        || QUuid(obj.value("id").toString()).toString(QUuid::WithoutBraces) != obj.value("id").toString()
        || obj.value("name").toString().trimmed().isEmpty()
        || obj.value("name").toString().size() > 80
        || obj.value("port").toInt() < 1 || obj.value("port").toInt() > 65535
        || !obj.value("time").isDouble()
        || !std::isfinite(obj.value("time").toDouble())
        || qAbs(double(QDateTime::currentSecsSinceEpoch()) - obj.value("time").toDouble()) > 60
        || !verify(tag, mac(p, QJsonDocument(obj).toJson(QJsonDocument::Compact), "discovery"))) return false;
    if (p.managed()) {
        const auto hex = obj.value("publicKey").toString().toLatin1();
        if (hex.size() != 64 || QByteArray::fromHex(hex).toHex() != hex) return false;
        // A holder of the discovery secret must not replace a pinned identity's
        // key in the device list and thereby obstruct its revocation.
        for (const auto &entry : p.registry.value("devices").toArray()) {
            const auto device = entry.toObject();
            if (device.value("id") == obj.value("id") && device.value("key").toString().toLatin1() != hex) return false;
        }
        const auto r = obj.value("registry").toObject();
        if (!r.isEmpty() && !DeviceAccess::validRegistry(p, r)) return false;
    }
    *object = obj;
    return true;
}
QByteArray SecureProtocol::seal(const Profile &p, const QByteArray &plain, const QByteArray &context) {
    if (!p.registered() || plain.size() > MaxWireBytes - 40) return {};
    QByteArray key = subkey(p, "transfer");
    QByteArray nonce = randomBytes(24), tag(16, Qt::Uninitialized), encrypted(plain.size(), Qt::Uninitialized);
    crypto_aead_lock(ptr(encrypted), ptr(tag), ptr(key), ptr(nonce), ptr(context), size_t(context.size()), ptr(plain), size_t(plain.size()));
    crypto_wipe(key.data(), size_t(key.size()));
    return nonce + tag + encrypted;
}
bool SecureProtocol::open(const Profile &p, const QByteArray &wire, const QByteArray &context, QByteArray *plain) {
    if (!plain || !p.registered() || wire.size() < 40 || wire.size() > MaxWireBytes) return false;
    QByteArray key = subkey(p, "transfer"), result(wire.size() - 40, Qt::Uninitialized);
    const int status = crypto_aead_unlock(ptr(result), ptr(wire) + 24, ptr(key), ptr(wire),
                                        ptr(context), size_t(context.size()), ptr(wire) + 40, size_t(result.size()));
    crypto_wipe(key.data(), size_t(key.size()));
    if (status != 0) return false;
    *plain = result;
    return true;
}

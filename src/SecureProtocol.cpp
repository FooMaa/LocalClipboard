#include "SecureProtocol.h"
#include "monocypher.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QtEndian>
#include <QUuid>

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
    QJsonObject obj{{"version", 2}, {"group", p.groupId()}, {"id", p.deviceId},
                    {"name", p.deviceName}, {"port", port}, {"time", double(QDateTime::currentSecsSinceEpoch())}};
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    obj.insert("mac", QString::fromLatin1(mac(p, body, "discovery").toHex()));
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}
bool SecureProtocol::readAnnouncement(const Profile &p, const QByteArray &bytes, QJsonObject *object) {
    if (bytes.size() > 2048 || !p.registered()) return false;
    QJsonObject obj = QJsonDocument::fromJson(bytes).object();
    const QByteArray tag = QByteArray::fromHex(obj.take("mac").toString().toLatin1());
    if (obj.value("version").toInt() != 2 || obj.value("group").toString() != p.groupId()
        || QUuid(obj.value("id").toString()).isNull()
        || obj.value("id").toString().size() != 36
        || obj.value("name").toString().size() > 80
        || obj.value("port").toInt() < 1 || obj.value("port").toInt() > 65535
        || !obj.value("time").isDouble()
        || qAbs(double(QDateTime::currentSecsSinceEpoch()) - obj.value("time").toDouble()) > 60
        || !verify(tag, mac(p, QJsonDocument(obj).toJson(QJsonDocument::Compact), "discovery"))) return false;
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
    if (!p.registered() || wire.size() < 40 || wire.size() > MaxWireBytes) return false;
    QByteArray key = subkey(p, "transfer"), result(wire.size() - 40, Qt::Uninitialized);
    const int status = crypto_aead_unlock(ptr(result), ptr(wire) + 24, ptr(key), ptr(wire),
                                        ptr(context), size_t(context.size()), ptr(wire) + 40, size_t(result.size()));
    crypto_wipe(key.data(), size_t(key.size()));
    if (status != 0) return false;
    *plain = result;
    return true;
}

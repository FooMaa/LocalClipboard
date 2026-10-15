#include "DeviceAccess.h"
#include "SecureProtocol.h"
#include "monocypher.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace {
const uint8_t *bytes(const QByteArray &v) { return reinterpret_cast<const uint8_t *>(v.constData()); }
uint8_t *bytes(QByteArray &v) { return reinterpret_cast<uint8_t *>(v.data()); }
QByteArray signingKey(const Profile &p, QByteArray *pub) {
    if (p.authoritySeed.size() != 32) return {};
    QByteArray seed = p.authoritySeed, secret(64, Qt::Uninitialized);
    pub->resize(32);
    crypto_eddsa_key_pair(bytes(secret), bytes(*pub), bytes(seed));
    crypto_wipe(seed.data(), size_t(seed.size()));
    return secret;
}
QByteArray body(const Profile &p, QJsonObject registry) {
    registry.remove("signature");
    return QByteArray("LocalClipboard/registry/v3/") + p.groupId().toUtf8() + QJsonDocument(registry).toJson(QJsonDocument::Compact);
}
QByteArray decode(const QString &hex) {
    const auto data = QByteArray::fromHex(hex.toLatin1());
    return hex.size() == 64 && data.size() == 32 && data.toHex() == hex.toLatin1() ? data : QByteArray();
}
}
QByteArray DeviceAccess::publicKey(const Profile &p) {
    if (p.deviceSecret.size() != 32) return {};
    QByteArray pub(32, Qt::Uninitialized);
    crypto_x25519_public_key(bytes(pub), bytes(p.deviceSecret));
    return pub;
}
bool DeviceAccess::initialize(Profile *p) {
    if (!p) return false;
    p->deviceSecret = SecureProtocol::randomBytes(32);
    p->authoritySeed = SecureProtocol::randomBytes(32);
    QByteArray pub, secret = signingKey(*p, &pub);
    crypto_wipe(secret.data(), size_t(secret.size()));
    p->authorityPublic = pub; p->registry = {};
    return authorize(p, p->deviceId, publicKey(*p), false);
}
bool DeviceAccess::isAdministrator(const Profile &p) {
    QByteArray pub, secret = signingKey(p, &pub);
    const bool valid = !secret.isEmpty() && pub == p.authorityPublic;
    crypto_wipe(secret.data(), size_t(secret.size()));
    return valid;
}
bool DeviceAccess::validRegistry(const Profile &p, const QJsonObject &r) {
    if (!p.managed() || r.size() != 3 || !r.value("devices").isArray()) return false;
    const double revision = r.value("revision").toDouble();
    if (!std::isfinite(revision) || revision < 1 || revision > 9007199254740991.0 || std::floor(revision) != revision) return false;
    const auto devices = r.value("devices").toArray();
    if (devices.isEmpty() || devices.size() > 64) return false;
    QSet<QString> ids; QSet<QByteArray> keys;
    for (const auto &v : devices) {
        const auto d = v.toObject(); const auto id = d.value("id").toString();
        const auto key = decode(d.value("key").toString());
        if (d.size() != 3 || QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id
            || key.isEmpty() || !d.value("revoked").isBool() || ids.contains(id) || keys.contains(key)) return false;
        ids.insert(id); keys.insert(key);
    }
    const auto encoded = r.value("signature").toString().toLatin1();
    const auto signature = QByteArray::fromHex(encoded); const auto message = body(p, r);
    return signature.size() == 64 && encoded == signature.toHex()
        && crypto_eddsa_check(bytes(signature), bytes(p.authorityPublic), bytes(message), size_t(message.size())) == 0;
}
bool DeviceAccess::updateRegistry(Profile *p, const QJsonObject &r) {
    if (!p || !validRegistry(*p, r) || r.value("revision").toDouble() <= p->registry.value("revision").toDouble()) return false;
    p->registry = r; return true;
}
bool DeviceAccess::authorize(Profile *p, const QString &id, const QByteArray &key, bool revoked) {
    if (!p || !isAdministrator(*p) || key.size() != 32) return false;
    QJsonArray devices = p->registry.value("devices").toArray();
    bool found = false;
    for (int i = 0; i < devices.size(); ++i) {
        auto d = devices[i].toObject();
        if (d.value("id").toString() != id) continue;
        // A revoked identity cannot substitute a new public key to restore access.
        if (d.value("key").toString() != QString::fromLatin1(key.toHex())) return false;
        if (d.value("revoked").toBool() && !revoked) return false;
        d.insert("revoked", revoked); devices[i] = d; found = true; break;
    }
    if (!found) devices.append(QJsonObject{{"id", id}, {"key", QString::fromLatin1(key.toHex())}, {"revoked", revoked}});
    QJsonObject r{{"revision", p->registry.value("revision").toDouble() + 1}, {"devices", devices}};
    QByteArray pub, secret = signingKey(*p, &pub), signature(64, Qt::Uninitialized);
    const auto message = body(*p, r);
    crypto_eddsa_sign(bytes(signature), bytes(secret), bytes(message), size_t(message.size()));
    crypto_wipe(secret.data(), size_t(secret.size()));
    r.insert("signature", QString::fromLatin1(signature.toHex()));
    if (!validRegistry(*p, r)) return false;
    p->registry = r; return true;
}
bool DeviceAccess::approved(const Profile &p, const QString &id, const QByteArray &key) {
    if (!p.managed()) return true;
    for (const auto &v : p.registry.value("devices").toArray()) {
        const auto d = v.toObject();
        if (d.value("id").toString() == id)
            return !d.value("revoked").toBool(true) && (key.isEmpty() || d.value("key").toString() == QString::fromLatin1(key.toHex()));
    }
    return false;
}
Profile DeviceAccess::channel(const Profile &p, const QString &id) {
    Profile result = p;
    if (!p.managed()) return result;
    result.key.clear();
    if (!approved(p, p.deviceId, publicKey(p)) || !approved(p, id)) return result;
    for (const auto &v : p.registry.value("devices").toArray()) {
        const auto d = v.toObject();
        if (d.value("id").toString() != id) continue;
        QByteArray shared(32, Qt::Uninitialized);
        const auto peer = decode(d.value("key").toString());
        crypto_x25519(bytes(shared), bytes(p.deviceSecret), bytes(peer));
        if (shared == QByteArray(32, '\0')) return result;
        // Bind the pairwise key to this profile; a discovery secret alone cannot decrypt transfers.
        result.key = QCryptographicHash::hash(QByteArray("LocalClipboard/channel/v3/") + shared + p.key, QCryptographicHash::Sha256);
        crypto_wipe(shared.data(), size_t(shared.size()));
        break;
    }
    return result;
}
QString DeviceAccess::fingerprint(const QByteArray &key) {
    return QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex().left(24));
}

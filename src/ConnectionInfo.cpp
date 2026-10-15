#include "ConnectionInfo.h"
#include <QNetworkInterface>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool ConnectionInfo::parse(const QString &text, ConnectionInfo *result) {
    if (!result || text.size() > 1024) return false;
    ConnectionInfo info;
    const QString clean = text.trimmed();
    if (clean.startsWith("LCQR1.")) {
        const QByteArray encoded = clean.mid(6).toLatin1();
        const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) return false;
        const auto object = QJsonDocument::fromJson(decoded.decoded).object();
        if (!object.value("code").isString() || !object.value("addresses").isArray()) return false;
        info.code = object.value("code").toString();
        for (const auto &value : object.value("addresses").toArray()) {
            if (!value.isString()) return false;
            info.addresses.append(value.toString());
        }
        QStringList checked;
        if (!Profile::parseAddresses(info.addresses.join(' '), &checked) || checked.size() != info.addresses.size()) return false;
        info.addresses = checked;
    } else info.code = clean;
    Profile check;
    if (!check.join(info.code)) return false;
    *result = info;
    return true;
}
ConnectionInfo ConnectionInfo::forProfile(const Profile &profile) {
    ConnectionInfo info{profile.invitation(), {}};
    for (const auto &network : QNetworkInterface::allInterfaces()) {
        if (!network.flags().testFlag(QNetworkInterface::IsUp) || network.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        for (const auto &entry : network.addressEntries()) {
            const auto address = entry.ip();
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback() || address.isLinkLocal()) continue;
            if (!info.addresses.contains(address.toString())) info.addresses.append(address.toString());
            if (info.addresses.size() == 8) return info;
        }
    }
    return info;
}
QString ConnectionInfo::encoded() const {
    if (addresses.isEmpty()) return code;
    return "LCQR1." + QString::fromLatin1(QJsonDocument(QJsonObject{{"code", code},
        {"addresses", QJsonArray::fromStringList(addresses)}}).toJson(QJsonDocument::Compact)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

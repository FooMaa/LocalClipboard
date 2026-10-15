#pragma once
#include "Profile.h"
#include <QJsonObject>

// A profile owner signs the device registry. Members cannot grant or restore access.
namespace DeviceAccess {
QByteArray publicKey(const Profile &profile);
bool initialize(Profile *profile);
bool isAdministrator(const Profile &profile);
bool validRegistry(const Profile &profile, const QJsonObject &registry);
bool updateRegistry(Profile *profile, const QJsonObject &registry);
bool authorize(Profile *profile, const QString &id, const QByteArray &publicKey, bool revoked);
bool approved(const Profile &profile, const QString &id, const QByteArray &publicKey = {});
Profile channel(const Profile &profile, const QString &peerId);
QString fingerprint(const QByteArray &publicKey);
}

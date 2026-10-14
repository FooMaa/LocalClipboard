#pragma once
#include "Profile.h"
#include <QJsonObject>

namespace SecureProtocol {
constexpr int MaxWireBytes = 48 * 1024 * 1024;
QByteArray randomBytes(int size);
QByteArray mac(const Profile &profile, const QByteArray &message, const QByteArray &domain);
bool verify(const QByteArray &a, const QByteArray &b);
QByteArray announcement(const Profile &profile, quint16 port);
bool readAnnouncement(const Profile &profile, const QByteArray &bytes, QJsonObject *object);
QByteArray seal(const Profile &profile, const QByteArray &plain, const QByteArray &context);
bool open(const Profile &profile, const QByteArray &wire, const QByteArray &context, QByteArray *plain);
}

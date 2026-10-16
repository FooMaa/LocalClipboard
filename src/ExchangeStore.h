#pragma once
#include "Profile.h"
#include <QVariantList>

// The home-screen inbox is bounded independently of optional clipboard history.
namespace ExchangeStore {
constexpr int MaxItems = 20;
constexpr qint64 MaxBytes = 64LL * 1024 * 1024;
QString record(const Profile &profile, const QJsonObject &payload, const QString &sender = {},
               const QString &transferId = {}, bool outgoing = false);
QVariantList entries(const Profile &profile);
QJsonObject item(const Profile &profile, const QString &id);
QString revision();
void clear();
}


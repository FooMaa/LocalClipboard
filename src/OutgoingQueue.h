#pragma once
#include "Profile.h"

// Pending explicit sends are independent of the Home list and expire in ten minutes.
namespace OutgoingQueue {
constexpr int MaxItems = 20;
constexpr qint64 MaxBytes = 64LL * 1024 * 1024;
bool enqueue(const Profile &profile, const QJsonObject &payload);
QJsonObject next(const Profile &profile);
void remove(const QString &id);
void prune();
void clear();
}

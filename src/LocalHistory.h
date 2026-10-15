#pragma once
#include "Profile.h"
#include <QVariantList>

namespace LocalHistory {
bool record(const Profile &profile, const QJsonObject &payload, QString *error = nullptr);
QVariantList entries(const Profile &profile, const QString &query = {});
QJsonObject payload(const Profile &profile, const QString &id);
void clear();
void prune(const Profile &profile);
}

#pragma once
#include "Profile.h"
#include <QList>
#include <QUrl>
namespace PendingFiles {
QList<QUrl> load(const Profile &profile, const QString &directory = {});
bool append(const Profile &profile, const QList<QUrl> &files, QString *error, const QString &directory = {});
void clear(const Profile &profile, const QString &directory = {});
}

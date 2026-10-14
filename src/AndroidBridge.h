#pragma once
#include "Profile.h"
#include <QJsonObject>
#include <QList>
#include <QUrl>
#include <functional>
class QCoreApplication;
class QWidget;
namespace AndroidBridge {
QString installationLanguage();
void setLanguage(const QString &language);
bool setEnabled(bool enabled);
void startService(bool bootEnabled);
void stopService();
bool queueOutgoing(const Profile &profile, const QJsonObject &payload, QString *error);
void consumeInbox(const Profile &profile, const std::function<void(const QJsonObject &)> &apply);
void consumeShares(const std::function<void(const QJsonObject &)> &send, const std::function<void(const QString &)> &error);
QStringList peerDescriptions(const Profile &profile);
void copyFiles(const QList<QUrl> &urls);
void openReceivedFiles(QWidget *parent);
int runService(QCoreApplication &app);
}

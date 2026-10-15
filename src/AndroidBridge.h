#pragma once
#include "Profile.h"
#include <QJsonObject>
#include <QList>
#include <QUrl>
#include <functional>
class QCoreApplication;
class QWidget;
class QObject;
namespace AndroidBridge {
bool hasPendingInbox();
bool takeCopyRequest();
void notificationSettings();
QString installationLanguage();
void setLanguage(const QString &language);
bool setEnabled(bool enabled);
void startService(bool bootEnabled);
void stopService();
void queueOutgoing(const Profile &profile, const QJsonObject &payload, QObject *owner, const std::function<void(const QString &)> &done);
void consumeInbox(const Profile &profile, QObject *owner, const std::function<void(const QJsonObject &)> &apply);
void acknowledgeInbox();
bool consumeShares(QObject *owner, const std::function<void(const QJsonObject &)> &send, const std::function<void(const QString &)> &error);
QStringList peerDescriptions(const Profile &profile);
void copyFiles(const QList<QUrl> &urls);
void openReceivedFiles(QWidget *parent);
void openFile(const QString &path);
int runService(QCoreApplication &app);
}

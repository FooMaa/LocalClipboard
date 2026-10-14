#pragma once
#include <QList>
#include <QUrl>
#include <QString>
#include <functional>
class QWidget;
struct Profile;
namespace IOSBridge {
using FilesReady = std::function<void(const QList<QUrl> &, const QString &)>;
// Copy security-scoped / provider URLs before their temporary access expires.
void importFiles(const QList<QUrl> &urls, FilesReady ready);
void pickFiles(QWidget *parent, FilesReady ready);
void copyFiles(const QList<QUrl> &urls);
void openReceivedFiles(QWidget *parent);
bool queueFiles(const Profile &profile, const QList<QUrl> &urls, QString *error);
QList<QUrl> queuedFiles(const Profile &profile);
void clearQueue(const Profile &profile);
}

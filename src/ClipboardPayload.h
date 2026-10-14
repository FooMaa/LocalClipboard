#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>
class QMimeData;

namespace ClipboardPayload {
constexpr int MaxContentBytes = 32 * 1024 * 1024;
constexpr int MaxFiles = 32;
QJsonObject capture(const QMimeData *mime, QString *error);
QJsonObject fromFiles(const QList<QUrl> &urls, QString *error);
bool validate(const QJsonObject &payload, QString *error);
QMimeData *materialize(const QJsonObject &payload, const QString &directory, QString *error);
QString hash(const QJsonObject &payload);
QString description(const QJsonObject &payload);
bool safeName(const QString &name);
}

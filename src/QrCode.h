#pragma once
#include <QImage>
#include <QString>
namespace QrCode {
QImage encodeInvitation(const QString &invitation);
QString decodeInvitation(const QImage &image);
}

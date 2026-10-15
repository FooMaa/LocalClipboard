#pragma once
#include <QStringList>
#include <QList>
class QWidget;
namespace AndroidUi {
int choose(QWidget *owner, const QString &title, const QStringList &items, const QList<bool> &enabled, int selected);
QString scanQr(QWidget *owner);
}

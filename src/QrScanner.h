#pragma once
#include <QString>
class QWidget;
namespace QrScanner {
// Returns a validated LC2 invitation, or empty on cancellation.
QString scan(QWidget *parent);
}

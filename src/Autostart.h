#pragma once
#include <QString>
namespace Autostart {
bool setEnabled(bool enabled, QString *error = nullptr);
bool startSessionService();
}

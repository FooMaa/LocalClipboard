#pragma once
#include <QString>
class QWidget;
namespace Uninstall {
QString ipcName();
bool requestQuit();
void launch(QWidget *parent);
int run();
}

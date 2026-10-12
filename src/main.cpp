#include "ClipboardSync.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("LocalClipboard"));
    QApplication::setOrganizationName(QStringLiteral("LocalClipboard"));

    QMessageBox warning;
    warning.setIcon(QMessageBox::Warning);
    warning.setWindowTitle(QStringLiteral("LocalClipboard — security notice"));
    warning.setText(QStringLiteral(
        "This prototype sends clipboard text unencrypted to peers on the local network. "
        "There is no device authentication. Use only on a trusted private Wi-Fi network "
        "and do not copy passwords, tokens, or sensitive information."));
    warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    warning.setDefaultButton(QMessageBox::Cancel);
    if (warning.exec() != QMessageBox::Ok) return 0;

    ClipboardSync window;
    window.show();
    return app.exec();
}

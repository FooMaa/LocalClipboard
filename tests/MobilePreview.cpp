#include "AppWindow.h"
#include "AppUi.h"
#include "I18n.h"
#include "Profile.h"
#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName("LocalClipboardPreview-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QTemporaryDir preferences;
    I18n::initialize(app.arguments().contains("--ru") ? "ru" : "en", preferences.path());
    initializeAppUi();
    AppWindow window(Profile::load());
    if (!window.window()) return 1;
    return app.exec();
}

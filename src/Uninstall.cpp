#include "Uninstall.h"
#include "Autostart.h"
#include "I18n.h"
#include "Profile.h"
#include "Ui.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLocalSocket>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
class RemovalDialog final : public QDialog {
public:
    bool busy = false;
    void reject() override { if (!busy) QDialog::reject(); }
protected:
    void closeEvent(QCloseEvent *event) override {
        if (busy) event->ignore(); else QDialog::closeEvent(event);
    }
};
}

QString Uninstall::ipcName() {
    return "localclipboard-" + QString::fromLatin1(QCryptographicHash::hash(
        Profile::dataDirectory().toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}
bool Uninstall::requestQuit() {
    QLocalSocket socket; socket.connectToServer(ipcName());
    if (!socket.waitForConnected(1000)) return true;
    if (socket.write("quit") != 4) return false;
    // A completed write is successful even when there is nothing left to wait for.
    return socket.bytesToWrite() == 0 || socket.waitForBytesWritten(1000);
}
void Uninstall::launch(QWidget *parent) {
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--uninstall"}))
        QMessageBox::warning(parent, I18n::text("Uninstall"), I18n::text("Could not start the uninstaller."));
}
int Uninstall::run() {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return 1;
#elif defined(Q_OS_WIN)
    const QString executable = QCoreApplication::applicationDirPath() + "/Uninstall.exe";
    if (QFile::exists(executable) && QProcess::startDetached(executable, {})) return 0;
    QMessageBox::warning(nullptr, I18n::text("Uninstall"), I18n::text("Install LocalClipboard using the Windows installer first."));
    return 1;
#else
    RemovalDialog dialog; Ui::title(&dialog, "Uninstall LocalClipboard");
    const auto form = Ui::form(&dialog, "Remove LocalClipboard", "The app and its automatic startup will be removed. Your profile and received files will be kept unless you choose to erase them.");
    auto *erase = Ui::checkBox("Erase my local data", &dialog); form.body->addWidget(erase);
    form.body->addWidget(Ui::label("This also erases your registration, received files, imported copies and language preference on this device. Other devices are unaffected.", &dialog, "muted"));
    auto *status = Ui::label("", &dialog, "error"); form.body->addWidget(status); form.body->addStretch();
    auto *buttons = new QVBoxLayout;
    auto *cancel = Ui::button("Cancel", &dialog); auto *remove = Ui::button("Uninstall", &dialog, true);
    buttons->addWidget(cancel); buttons->addWidget(remove); form.footer->addLayout(buttons);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
#ifdef Q_OS_MACOS
        const QDir executableDir(QCoreApplication::applicationDirPath());
        const QString bundle = QDir::cleanPath(executableDir.absoluteFilePath("../.."));
        if (!bundle.endsWith(".app") || !QFile::exists(bundle + "/Contents/Info.plist")) {
            status->setText(I18n::text("Could not remove LocalClipboard.")); return;
        }
        QTemporaryDir temporary;
        const QString helper = temporary.path() + "/uninstall.sh";
        if (!temporary.isValid() || !QFile::copy(":/uninstall/uninstall.sh", helper)) {
            status->setText(I18n::text("Cannot create the uninstall helper.")); return;
        }
        QFile::setPermissions(helper, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QProcess validation;
        validation.start("/bin/bash", {helper, "--dry-run", bundle, Profile::dataDirectory(),
            QString::number(QCoreApplication::applicationPid()), I18n::language(), erase->isChecked() ? "erase" : "keep"});
        if (!validation.waitForFinished(5000) || validation.exitStatus() != QProcess::NormalExit || validation.exitCode() != 0) {
            status->setText(I18n::text("Could not remove LocalClipboard.")); return;
        }
        QString error;
        if (!Autostart::setEnabled(false, &error)) { status->setText(I18n::text(error)); return; }
        if (!QProcess::startDetached("/bin/bash", {helper, bundle, Profile::dataDirectory(),
            QString::number(QCoreApplication::applicationPid()), I18n::language(), erase->isChecked() ? "erase" : "keep"})) {
            status->setText(I18n::text("Could not start the uninstaller.")); return;
        }
        temporary.setAutoRemove(false);
        requestQuit(); dialog.accept();
#else
        const QString pkexec = QStandardPaths::findExecutable("pkexec"), apt = QStandardPaths::findExecutable("apt-get");
        if (pkexec.isEmpty() || apt.isEmpty()) { status->setText(I18n::text("Package removal requires pkexec (PolicyKit) and apt-get.")); return; }
        QString error;
        if (!Autostart::setEnabled(false, &error)) { status->setText(I18n::text(error)); return; }
        requestQuit();
        QProcess::execute("systemctl", {"--user", "stop", "localclipboard.service"});
        QProcess::execute("systemctl", {"--user", "daemon-reload"});
        auto *process = new QProcess(&dialog);
        dialog.busy = true;
        remove->setEnabled(false); cancel->setEnabled(false); erase->setEnabled(false);
        status->setText(I18n::text("Removing…"));
        QObject::connect(process, &QProcess::errorOccurred, &dialog, [=, &dialog](QProcess::ProcessError problem) {
            if (problem != QProcess::FailedToStart) return;
            dialog.busy = false;
            status->setText(I18n::text("Could not start the uninstaller.")); remove->setEnabled(true); cancel->setEnabled(true); erase->setEnabled(true);
        });
        QObject::connect(process, &QProcess::finished, &dialog, [=, &dialog](int exitCode, QProcess::ExitStatus result) {
            dialog.busy = false;
            if (exitCode != 0 || result != QProcess::NormalExit) {
                status->setText(I18n::text("Could not remove LocalClipboard.") + "\n" + QString::fromUtf8(process->readAllStandardError()).right(1000));
                remove->setEnabled(true); cancel->setEnabled(true); erase->setEnabled(true); return;
            }
            if (erase->isChecked() && !QDir(Profile::dataDirectory()).removeRecursively()) {
                status->setText(I18n::text("Cannot remove local data: ") + Profile::dataDirectory()); cancel->setEnabled(true); return;
            }
            QMessageBox::information(&dialog, "LocalClipboard", I18n::text("LocalClipboard was removed.") + "\n" +
                I18n::text(erase->isChecked() ? "Your local profile and files were erased." : "Your profile and files were kept."));
            dialog.accept();
        });
        process->start(pkexec, {apt, "remove", "-y", "localclipboard"});
#endif
    });
    return dialog.exec() == QDialog::Accepted ? 0 : 1;
#endif
}

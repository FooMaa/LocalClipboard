#include "Registration.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>

bool registerDevice(Profile *profile, QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Register this device");
    auto *layout = new QFormLayout(&dialog);
    auto *intro = new QLabel("Create your personal profile on the first device. On your other devices, enter its connection code. "
                            "Each person should create a separate profile, even on the same Wi-Fi.", &dialog);
    intro->setWordWrap(true);
    layout->addRow(intro);
    auto *device = new QLineEdit(profile->deviceName, &dialog);
    auto *owner = new QLineEdit(profile->ownerName, &dialog);
    device->setMaxLength(80); owner->setMaxLength(80);
    auto *mode = new QComboBox(&dialog);
    mode->addItems({"Create a personal profile", "Connect to my existing profile"});
    auto *code = new QLineEdit(&dialog);
    code->setPlaceholderText("LC2.…"); code->setMaxLength(68); code->setEnabled(false);
    auto *startup = new QCheckBox("Start in background when I log in / restart the device", &dialog);
    startup->setChecked(true);
#ifdef Q_OS_IOS
    startup->setChecked(false);
    startup->hide();
#endif
    layout->addRow("Device name", device); layout->addRow("Your name", owner);
    layout->addRow("Profile", mode); layout->addRow("Connection code", code); layout->addRow(startup);
#ifdef Q_OS_IOS
    startup->hide();
    auto *iosNote = new QLabel("Keep LocalClipboard open to receive. Send with the clipboard button, Files / Photos, or Open in LocalClipboard. "
        "Add a computer's local IP in Network settings.", &dialog);
    iosNote->setWordWrap(true); layout->addRow(iosNote);
#endif
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    QObject::connect(mode, &QComboBox::currentIndexChanged, code, [code](int index) { code->setEnabled(index == 1); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        Profile candidate = *profile;
        candidate.deviceName = device->text().trimmed(); candidate.ownerName = owner->text().trimmed();
        candidate.autostart = startup->isChecked(); candidate.paused = false;
        if (candidate.deviceName.isEmpty() || candidate.ownerName.isEmpty()) {
            QMessageBox::information(&dialog, "Registration", "Enter your name and a device name."); return;
        }
        if (mode->currentIndex() == 0) candidate.create();
        else if (!candidate.join(code->text())) { QMessageBox::information(&dialog, "Registration", "Enter the complete connection code from your other device."); return; }
        QString error;
        if (!candidate.save(&error)) { QMessageBox::critical(&dialog, "Registration", error); return; }
        *profile = candidate;
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return false;
    if (mode->currentIndex() == 0) showInvitation(*profile, parent);
    return true;
}
void showInvitation(const Profile &profile, QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Connect another device");
    auto *layout = new QFormLayout(&dialog);
    auto *label = new QLabel("Enter this code when registering your other devices. Anyone with this code can join your profile. "
                            "Keep it private; another person should create their own profile.", &dialog);
    label->setWordWrap(true); layout->addRow(label);
    auto *code = new QLineEdit(profile.invitation(), &dialog);
    code->setReadOnly(true); layout->addRow("Connection code", code);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

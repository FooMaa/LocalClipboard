#include "Registration.h"
#include "I18n.h"
#include "Ui.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QSignalBlocker>

RegistrationDialog::RegistrationDialog(const Profile &profile, QWidget *parent) : QDialog(parent), m_profile(profile) {
    setObjectName("registration");
    Ui::title(this, "Register this device");
    const auto form = Ui::form(this, "Your devices, together", "Create a personal profile on your first device. Use its connection code on your other devices. Each person needs their own profile, even on the same Wi-Fi.");
    auto *device = new QLineEdit(profile.deviceName, this); device->setObjectName("deviceName");
    auto *owner = new QLineEdit(profile.ownerName, this); owner->setObjectName("ownerName");
    device->setMaxLength(80); owner->setMaxLength(80);
    auto *mode = new QComboBox(this); mode->setObjectName("profileMode");
    mode->addItems({QString(), QString()});
    mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    const auto updateMode = [mode] {
        const QSignalBlocker blocker(mode);
        mode->setItemText(0, I18n::text("Create a profile"));
        mode->setItemText(1, I18n::text("Connect to a profile"));
    };
    updateMode(); connect(&I18n::instance(), &I18n::languageChanged, this, updateMode);
    auto *code = new QLineEdit(this); code->setObjectName("connectionCode");
    code->setPlaceholderText("LC2.…"); code->setMaxLength(68); code->setEnabled(false);
    Ui::field(form.body, "Device name", device); Ui::field(form.body, "Your name", owner);
    Ui::field(form.body, "Profile", mode); Ui::field(form.body, "Connection code", code);
    auto *startup = Ui::checkBox("Start automatically", this); startup->setChecked(true);
#ifdef Q_OS_IOS
    startup->setChecked(false);
    startup->hide();
    form.body->addWidget(Ui::label("Keep LocalClipboard open to receive. Send using the clipboard button, Files / Photos, or Open in LocalClipboard. Add your computer's local IP in Network settings.", this, "muted"));
#else
    form.body->addWidget(startup);
    form.body->addWidget(Ui::label("Receive in the background after login or device restart.", this, "muted"));
#endif
    auto *errorLabel = Ui::label("", this, "error"); errorLabel->setObjectName("registrationError"); form.body->addWidget(errorLabel);
    form.body->addStretch();
    auto *buttons = new QVBoxLayout;
    auto *cancel = Ui::button("Cancel", this); auto *save = Ui::button("Register", this, true); save->setObjectName("registerButton");
    save->setDefault(true); buttons->addWidget(cancel); buttons->addWidget(save); form.footer->addLayout(buttons);
    connect(mode, &QComboBox::currentIndexChanged, code, [code](int index) { code->setEnabled(index == 1); });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [=] {
        Profile candidate = m_profile;
        candidate.deviceName = device->text().trimmed(); candidate.ownerName = owner->text().trimmed();
        candidate.autostart = startup->isChecked(); candidate.paused = false;
        if (candidate.deviceName.isEmpty() || candidate.ownerName.isEmpty()) {
            errorLabel->setText(I18n::text("Enter your name and a device name.")); return;
        }
        if (mode->currentIndex() == 0) candidate.create();
        else if (!candidate.join(code->text())) { errorLabel->setText(I18n::text("Enter the complete connection code from your other device.")); return; }
        QString error;
        if (!candidate.save(&error)) { errorLabel->setText(I18n::text(error)); return; }
        m_profile = candidate; m_created = mode->currentIndex() == 0; accept();
    });
}
bool registerDevice(Profile *profile, QWidget *parent) {
    RegistrationDialog dialog(*profile, parent);
    if (dialog.exec() != QDialog::Accepted) return false;
    *profile = dialog.profile();
    if (dialog.created()) showInvitation(*profile, parent);
    return true;
}
void showInvitation(const Profile &profile, QWidget *parent) {
    QDialog dialog(parent); Ui::title(&dialog, "Connect another device");
    const auto form = Ui::form(&dialog, "Connect another device", "Enter this code on your other devices. Anyone with the code can join your profile. Keep it private; another person should create their own profile.");
    auto *code = new QLineEdit(profile.invitation(), &dialog); code->setReadOnly(true);
    Ui::field(form.body, "Connection code", code);
    form.body->addWidget(Ui::label("Click the code, then select all and copy. The entire code is needed.", &dialog, "muted")); form.body->addStretch();
    auto *close = Ui::button("Close", &dialog, true); form.footer->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

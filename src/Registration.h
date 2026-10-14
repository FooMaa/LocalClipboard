#pragma once
#include "Profile.h"
#include <QDialog>
class RegistrationDialog final : public QDialog {
public:
    explicit RegistrationDialog(const Profile &profile, QWidget *parent = nullptr);
    Profile profile() const { return m_profile; }
    bool created() const { return m_created; }
private:
    Profile m_profile;
    bool m_created = false;
};
bool registerDevice(Profile *profile, QWidget *parent = nullptr);
void showInvitation(const Profile &profile, QWidget *parent = nullptr);

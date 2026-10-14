#pragma once
#include "Profile.h"
class QWidget;
bool registerDevice(Profile *profile, QWidget *parent = nullptr);
void showInvitation(const Profile &profile, QWidget *parent = nullptr);

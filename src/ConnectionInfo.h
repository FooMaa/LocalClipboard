#pragma once
#include "Profile.h"

// QR invitations add optional LAN hints without changing the LC2 profile key.
struct ConnectionInfo {
    QString code;
    QStringList addresses;
    static bool parse(const QString &text, ConnectionInfo *result);
    static ConnectionInfo forProfile(const Profile &profile);
    QString encoded() const;
};

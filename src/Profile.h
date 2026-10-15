#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QJsonObject>

struct Profile {
    QString deviceId;
    QString deviceName;
    QString ownerName;
    QByteArray key;
    bool autostart = true;
    bool paused = false;
    QStringList peerAddresses;
    QByteArray deviceSecret, authoritySeed, authorityPublic;
    QJsonObject registry;
    bool sendingEnabled = true, receivingEnabled = true, historyEnabled = false;
    qint64 pauseUntil = 0;
    int historyDays = 7, historyMiB = 16;
    QStringList excludedApplications;
    bool managed() const { return authorityPublic.size() == 32 && deviceSecret.size() == 32; }
    bool isPaused() const;
    void createManaged();

    bool registered() const;
    QString groupId() const;
    QString invitation() const;
    bool join(const QString &code);
    void create();
    bool save(QString *error = nullptr, const QString &directory = {}) const;
    static Profile load(const QString &directory = {});
    static QString dataDirectory();
    static bool parseAddresses(const QString &value, QStringList *addresses);
};

#pragma once
#include <QByteArray>
#include <QString>

struct Profile {
    QString deviceId;
    QString deviceName;
    QString ownerName;
    QByteArray key;
    bool autostart = true;
    bool paused = false;

    bool registered() const;
    QString groupId() const;
    QString invitation() const;
    bool join(const QString &code);
    void create();
    bool save(QString *error = nullptr, const QString &directory = {}) const;
    static Profile load(const QString &directory = {});
    static QString dataDirectory();
};

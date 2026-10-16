#pragma once
#include "Profile.h"

// Shared on-disk format for private payloads. Callers hold their directory lock
// while reading, writing or pruning; retention policy belongs to each store.
class PayloadStorage final {
public:
    explicit PayloadStorage(const QString &name);
    QString directory() const { return m_directory; }
    bool prepare() const;
    QJsonObject read(const Profile &profile, const QString &id) const;
    QByteArray encode(const Profile &profile, const QString &id, const QJsonObject &item) const;
    bool write(const QString &id, const QByteArray &wire) const;
    static QString newId();
    static bool validId(const QString &id);
private:
    QString m_directory;
    QByteArray m_context;
};

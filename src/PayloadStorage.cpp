#include "PayloadStorage.h"
#include "SecureProtocol.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

namespace {
Profile storageKey(Profile profile) {
    if (profile.managed()) profile.key = profile.deviceSecret;
    return profile;
}
}

PayloadStorage::PayloadStorage(const QString &name)
    : m_directory(QDir(Profile::dataDirectory()).filePath(name)),
      m_context("LocalClipboard/" + name.toUtf8() + "/v1/") {}

bool PayloadStorage::prepare() const {
    return !QFileInfo(m_directory).isSymLink() && QDir().mkpath(m_directory)
        && QFile::setPermissions(m_directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

QString PayloadStorage::newId() {
    return QString::number(QDateTime::currentMSecsSinceEpoch()) + "-"
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool PayloadStorage::validId(const QString &id) {
    static const QRegularExpression valid("^[0-9]{13}-[0-9a-f-]{36}$");
    return valid.match(id).hasMatch();
}

QByteArray PayloadStorage::encode(const Profile &profile, const QString &id, const QJsonObject &item) const {
    if (!profile.registered() || !validId(id)) return {};
    return SecureProtocol::seal(storageKey(profile), QJsonDocument(item).toJson(QJsonDocument::Compact), m_context + id.toUtf8());
}

QJsonObject PayloadStorage::read(const Profile &profile, const QString &id) const {
    if (!profile.registered() || !validId(id) || QFileInfo(m_directory).isSymLink()) return {};
    const QFileInfo info(QDir(m_directory).filePath(id));
    if (info.isSymLink() || !info.isFile() || info.size() > SecureProtocol::MaxWireBytes) return {};
    QFile file(info.filePath());
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray plain;
    if (!SecureProtocol::open(storageKey(profile), file.readAll(), m_context + id.toUtf8(), &plain)) return {};
    const auto item = QJsonDocument::fromJson(plain).object();
    return item.value("group").toString() == profile.groupId() ? item : QJsonObject();
}

bool PayloadStorage::write(const QString &id, const QByteArray &wire) const {
    if (!validId(id) || wire.isEmpty() || wire.size() > SecureProtocol::MaxWireBytes) return false;
    QSaveFile file(QDir(m_directory).filePath(id));
    return file.open(QIODevice::WriteOnly)
        && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        && file.write(wire) == wire.size() && file.commit();
}

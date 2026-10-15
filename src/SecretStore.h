#pragma once
#include <QByteArray>
#include <QString>

namespace SecretStore {
// Android wraps the profile secret with the app's non-exportable Keystore key.
// Desktop storage is protected by the per-user profile directory and file permissions.
QString encode(const QByteArray &key);
QByteArray decode(const QString &stored);
}

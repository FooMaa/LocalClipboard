#pragma once
#include <QString>

namespace FileStorage {
constexpr qint64 MaxReceivedBytes = 512LL * 1024 * 1024;
constexpr int MaxReceivedFiles = 16384;
bool canReceive(const QString &directory, qint64 bytes, int files, QString *error);
bool isReceivedFile(const QString &path, const QString &directory);
}

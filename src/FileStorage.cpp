#include "FileStorage.h"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

bool FileStorage::isReceivedFile(const QString &path, const QString &directory) {
    const QFileInfo file(path);
    const QString root = QFileInfo(directory).canonicalFilePath();
    return !root.isEmpty() && file.isFile() && !file.isSymLink()
        && file.canonicalFilePath().startsWith(root + '/');
}
bool FileStorage::canReceive(const QString &directory, qint64 bytes, int files, QString *error) {
    const auto fail = [error] { if (error) *error = "Received files storage is full. Delete local copies to receive more files."; return false; };
    if (bytes < 0 || files < 1 || bytes > MaxReceivedBytes || files > MaxReceivedFiles) return fail();
    const QDir root(directory);
    qint64 total = bytes;
    int count = files;
    // Transfers are stored one directory deep. Never follow symbolic links or unbounded directory trees.
    for (const auto &batch : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        for (const auto &file : QDir(batch.absoluteFilePath()).entryInfoList(QDir::Files | QDir::NoSymLinks)) {
            total += file.size(); ++count;
            if (total > MaxReceivedBytes || count > MaxReceivedFiles) return fail();
        }
    }
    const QStorageInfo storage(directory);
    if (storage.isValid() && storage.bytesAvailable() >= 0 && storage.bytesAvailable() < bytes + 64LL * 1024 * 1024) return fail();
    return true;
}

#include "OutgoingQueue.h"
#include "ClipboardPayload.h"
#include "PayloadStorage.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex mutex;
constexpr qint64 RetentionMs = 10LL * 60 * 1000;

QFileInfoList pruneExpired(const PayloadStorage &storage) {
    auto files = QDir(storage.directory()).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    const auto cutoff = QDateTime::currentMSecsSinceEpoch() - RetentionMs;
    for (auto it = files.begin(); it != files.end();) {
        if (!PayloadStorage::validId(it->fileName()) || it->fileName().left(13).toLongLong() < cutoff) {
            QFile::remove(it->filePath());
            it = files.erase(it);
        } else {
            ++it;
        }
    }
    return files;
}
}

bool OutgoingQueue::enqueue(const Profile &profile, const QJsonObject &payload) {
    QString error;
    if (!profile.registered() || !ClipboardPayload::validate(payload, &error)) return false;
    const PayloadStorage storage("outgoing");
    const auto id = PayloadStorage::newId();
    const auto wire = storage.encode(profile, id, {{"group", profile.groupId()}, {"payload", payload}});
    const QMutexLocker guard(&mutex);
    if (wire.isEmpty() || wire.size() > MaxBytes || !storage.prepare()) return false;
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return false;
    const auto pending = pruneExpired(storage);
    qint64 bytes = wire.size();
    for (const auto &file : pending) bytes += file.size();
    if (pending.size() >= MaxItems || bytes > MaxBytes) return false;
    return storage.write(id, wire);
}

QJsonObject OutgoingQueue::next(const Profile &profile) {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("outgoing");
    if (!storage.prepare()) return {};
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return {};
    for (const auto &file : pruneExpired(storage)) {
        auto item = storage.read(profile, file.fileName());
        if (item.isEmpty()) {
            QFile::remove(file.filePath());
            continue;
        }
        item.insert("id", file.fileName());
        return item;
    }
    return {};
}

void OutgoingQueue::remove(const QString &id) {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("outgoing");
    if (!PayloadStorage::validId(id) || !storage.prepare()) return;
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return;
    QFile::remove(QDir(storage.directory()).filePath(id));
}

void OutgoingQueue::prune() {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("outgoing");
    if (!storage.prepare()) return;
    QLockFile lock(storage.directory() + ".lock");
    if (lock.tryLock(1000)) pruneExpired(storage);
}

void OutgoingQueue::clear() {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("outgoing");
    if (!storage.prepare()) return;
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return;
    for (const auto &file : pruneExpired(storage)) QFile::remove(file.filePath());
}

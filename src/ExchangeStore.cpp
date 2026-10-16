#include "ExchangeStore.h"
#include "ClipboardPayload.h"
#include "PayloadStorage.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QLockFile>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex mutex;
constexpr qint64 RetentionMs = 7LL * 24 * 60 * 60 * 1000;
QFileInfoList files(const PayloadStorage &storage) {
    return QDir(storage.directory()).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name | QDir::Reversed);
}
void prune(const PayloadStorage &storage) {
    qint64 bytes = 0;
    int count = 0;
    const auto cutoff = QDateTime::currentMSecsSinceEpoch() - RetentionMs;
    for (const auto &file : files(storage)) {
        const auto time = file.fileName().left(13).toLongLong();
        if (!PayloadStorage::validId(file.fileName()) || time < cutoff || count >= ExchangeStore::MaxItems || bytes + file.size() > ExchangeStore::MaxBytes)
            QFile::remove(file.filePath());
        else {
            bytes += file.size();
            ++count;
        }
    }
}
QString thumbnail(const QJsonObject &payload) {
    QByteArray bytes;
    if (payload.value("kind") == "image") bytes = QByteArray::fromBase64(payload.value("png").toString().toLatin1());
    else if (payload.value("kind") == "files") {
        const auto list = payload.value("files").toArray();
        if (!list.isEmpty()) bytes = QByteArray::fromBase64(list.first().toObject().value("data").toString().toLatin1());
    }
    if (bytes.isEmpty()) return {};
    QBuffer source(&bytes);
    source.open(QIODevice::ReadOnly);
    QImageReader reader(&source);
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 8192 || size.height() > 8192 || qint64(size.width()) * size.height() > 32000000) return {};
    reader.setScaledSize(size.scaled(320, 200, Qt::KeepAspectRatio));
    const auto image = reader.read();
    if (image.isNull()) return {};
    QByteArray png;
    QBuffer output(&png);
    output.open(QIODevice::WriteOnly);
    image.save(&output, "PNG");
    return "data:image/png;base64," + QString::fromLatin1(png.toBase64());
}
QJsonObject metadata(const QJsonObject &payload) {
    QString preview = payload.value("text").toString().left(500);
    qint64 bytes = payload.value("text").toString().toUtf8().size();
    if (payload.value("kind") == "files") {
        QStringList names;
        bytes = 0;
        for (const auto &entry : payload.value("files").toArray()) {
            const auto file = entry.toObject();
            names.append(file.value("name").toString());
            bytes += qint64(file.value("size").toDouble());
        }
        preview = names.join(", ").left(500);
    } else if (payload.value("kind") == "image") bytes = QByteArray::fromBase64(payload.value("png").toString().toLatin1()).size();
    return {{"preview", preview}, {"bytes", double(bytes)}, {"thumbnail", thumbnail(payload)}};
}
}

QString ExchangeStore::record(const Profile &p, const QJsonObject &payload, const QString &sender, const QString &transferId, bool outgoing) {
    QString error;
    if (!p.registered() || !ClipboardPayload::validate(payload, &error)) return {};
    auto obj = metadata(payload);
    obj.insert("group", p.groupId());
    obj.insert("payload", payload);
    obj.insert("sender", sender);
    obj.insert("transferId", transferId);
    obj.insert("outgoing", outgoing);
    const PayloadStorage storage("exchange");
    const auto id = PayloadStorage::newId();
    const auto wire = storage.encode(p, id, obj);
    if (wire.isEmpty() || wire.size() > MaxBytes) return {};
    const QMutexLocker guard(&mutex);
    if (!storage.prepare()) return {};
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return {};
    if (!storage.write(id, wire)) return {};
    prune(storage);
    return QFile::exists(QDir(storage.directory()).filePath(id)) ? id : QString();
}

QVariantList ExchangeStore::entries(const Profile &p) {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("exchange");
    QVariantList result;
    if (!storage.prepare()) return result;
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return result;
    prune(storage);
    for (const auto &file : files(storage)) {
        const auto obj = storage.read(p, file.fileName());
        if (obj.isEmpty()) continue;
        result.append(QVariantMap{{"id", file.fileName()}, {"kind", obj.value("payload").toObject().value("kind").toString()},
            {"text", obj.value("preview").toString()}, {"thumbnail", obj.value("thumbnail").toString()},
            {"bytes", obj.value("bytes").toDouble()}, {"outgoing", obj.value("outgoing").toBool()},
            {"time", file.fileName().left(13).toLongLong()}});
    }
    return result;
}

QJsonObject ExchangeStore::item(const Profile &p, const QString &id) {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("exchange");
    if (!storage.prepare()) return {};
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return {};
    prune(storage);
    return storage.read(p, id);
}

QString ExchangeStore::revision() {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("exchange");
    if (QFileInfo(storage.directory()).isSymLink()) return {};
    QStringList names;
    bool expired = false;
    const auto cutoff = QDateTime::currentMSecsSinceEpoch() - RetentionMs;
    for (const auto &file : files(storage)) {
        names.append(file.fileName());
        if (file.fileName().left(13).toLongLong() < cutoff) expired = true;
    }
    if (expired) names.append("expired"); // Force a refresh when age changes without a new delivery.
    return names.join('/');
}

void ExchangeStore::clear() {
    const QMutexLocker guard(&mutex);
    const PayloadStorage storage("exchange");
    if (!storage.prepare()) return;
    QLockFile lock(storage.directory() + ".lock");
    if (!lock.tryLock(1000)) return;
    for (const auto &file : files(storage)) QFile::remove(file.filePath());
}

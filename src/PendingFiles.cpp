#include "PendingFiles.h"
#include "ClipboardPayload.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
QDir root(const QString &directory) { return QDir(directory.isEmpty() ? Profile::dataDirectory() : directory); }
QString path(const Profile &profile, const QString &directory) {
    return root(directory).filePath("outgoing-" + profile.groupId() + ".json");
}
bool importedFile(const QDir &base, const QUrl &url) {
    if (!url.isLocalFile()) return false;
    const QFileInfo info(url.toLocalFile());
    const QString imported = QFileInfo(base.filePath("imported")).canonicalFilePath();
    return !imported.isEmpty() && info.isFile() && !info.isSymLink()
        && info.canonicalFilePath().startsWith(imported + '/');
}
}
QList<QUrl> PendingFiles::load(const Profile &profile, const QString &directory) {
    if (!profile.registered()) return {};
    QFile file(path(profile, directory));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32768) return {};
    const auto base = root(directory);
    QList<QUrl> urls;
    const auto entries = QJsonDocument::fromJson(file.readAll()).array();
    if (entries.size() > ClipboardPayload::MaxFiles) return {};
    for (const auto &value : entries) {
        const QString relative = value.toString();
        if (!relative.startsWith("imported/") || QDir::cleanPath(relative) != relative) return {};
        const QUrl url = QUrl::fromLocalFile(base.filePath(relative));
        // A user may delete an imported copy in the file browser before sending.
        if (importedFile(base, url) && !urls.contains(url)) urls.append(url);
    }
    return urls;
}
bool PendingFiles::append(const Profile &profile, const QList<QUrl> &files, QString *error, const QString &directory) {
    if (!profile.registered()) { if (error) *error = "Register this device first."; return false; }
    const auto base = root(directory);
    QList<QUrl> urls = load(profile, directory);
    for (const auto &url : files) {
        if (!importedFile(base, url)) { if (error) *error = "Only imported local file copies can be queued."; return false; }
        if (!urls.contains(url)) urls.append(url);
    }
    qint64 bytes = 0;
    for (const auto &url : urls) bytes += QFileInfo(url.toLocalFile()).size();
    if (urls.isEmpty() || urls.size() > ClipboardPayload::MaxFiles || bytes > ClipboardPayload::MaxContentBytes) {
        if (error) *error = "Pending files exceed 32 files / 32 MiB. Send pending files first; local copies remain available for retry.";
        return false;
    }
    QJsonArray paths;
    for (const auto &url : urls) paths.append(base.relativeFilePath(url.toLocalFile()));
    const QByteArray data = QJsonDocument(paths).toJson(QJsonDocument::Compact);
    if (data.size() > 32768) { if (error) *error = "Too many long file paths."; return false; }
    QSaveFile file(path(profile, directory));
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(data) != data.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
void PendingFiles::clear(const Profile &profile, const QString &directory) { QFile::remove(path(profile, directory)); }

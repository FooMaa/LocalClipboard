#include "ClipboardPayload.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
QByteArray decode(const QString &text) {
    if (text.size() > (ClipboardPayload::MaxContentBytes + 2) / 3 * 4) return {};
    return QByteArray::fromBase64(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
}
bool imageSize(const QByteArray &bytes) {
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "PNG");
    const QSize size = reader.size();
    return size.isValid() && size.width() <= 8192 && size.height() <= 8192
        && qint64(size.width()) * size.height() <= 32 * 1024 * 1024;
}
}
bool ClipboardPayload::safeName(const QString &name) {
    if (name.isEmpty() || name.size() > 128 || name == "." || name == ".."
        || name.endsWith('.') || name.endsWith(' ')) return false;
    if (QRegularExpression(QStringLiteral("[\\x00-\\x1f<>:\"/\\\\|?*]")).match(name).hasMatch()) return false;
    return !QRegularExpression(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)"),
                               QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
}
QJsonObject ClipboardPayload::fromFiles(const QList<QUrl> &urls, QString *error) {
    if (urls.isEmpty() || urls.size() > MaxFiles) { fail(error, "Select between 1 and 32 files."); return {}; }
    QJsonArray files;
    QSet<QString> names;
    qint64 total = 0;
    for (const QUrl &url : urls) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (!url.isLocalFile() && url.scheme() != "content") { fail(error, "Only local files can be transferred."); return {}; }
        QFileInfo info(path);
        if (info.isDir() || info.isSymLink() || (url.isLocalFile() && !info.isFile())) {
            fail(error, "Only regular files are supported; archive folders first."); return {};
        }
        QString name = info.fileName();
        if (!safeName(name)) name = QStringLiteral("file-%1").arg(files.size() + 1);
        const QString original = name;
        for (int suffix = 2; names.contains(name.toCaseFolded()); ++suffix)
            name = QStringLiteral("%1-%2").arg(original.left(110)).arg(suffix);
        names.insert(name.toCaseFolded());
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { fail(error, "Cannot read " + name + ": " + file.errorString()); return {}; }
        const QByteArray bytes = file.read(MaxContentBytes - total + 1);
        total += bytes.size();
        if (total > MaxContentBytes || !file.atEnd()) { fail(error, "Files exceed the combined 32 MiB limit."); return {}; }
        files.append(QJsonObject{{"name", name}, {"data", QString::fromLatin1(bytes.toBase64())}, {"size", bytes.size()}});
    }
    return QJsonObject{{"kind", "files"}, {"files", files}};
}
QJsonObject ClipboardPayload::capture(const QMimeData *mime, QString *error) {
    if (!mime) return {};
    if (mime->hasUrls()) {
        bool local = true;
        for (const auto &url : mime->urls()) local &= url.isLocalFile() || url.scheme() == "content";
        if (local && !mime->urls().isEmpty()) return fromFiles(mime->urls(), error);
    }
    if (mime->hasImage()) {
        QImage image = qvariant_cast<QImage>(mime->imageData());
        if (image.isNull()) image = qvariant_cast<QPixmap>(mime->imageData()).toImage();
        if (image.isNull() || image.width() > 8192 || image.height() > 8192
            || qint64(image.width()) * image.height() > 32 * 1024 * 1024) {
            fail(error, "Image is invalid or exceeds 32 megapixels / 8192 pixels per side."); return {};
        }
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG") || bytes.size() > MaxContentBytes) { fail(error, "PNG exceeds 32 MiB."); return {}; }
        return QJsonObject{{"kind", "image"}, {"png", QString::fromLatin1(bytes.toBase64())}};
    }
    if (mime->hasText() || mime->hasHtml()) {
        const QString text = mime->text(), html = mime->html();
        if (text.trimmed().startsWith("LC2.")) return {};
        if (text.isEmpty() && html.isEmpty()) return {};
        if (text.toUtf8().size() + html.toUtf8().size() > MaxContentBytes) { fail(error, "Text exceeds 32 MiB."); return {}; }
        return QJsonObject{{"kind", "text"}, {"text", text}, {"html", html}};
    }
    return {};
}
bool ClipboardPayload::validate(const QJsonObject &p, QString *error) {
    const QString kind = p.value("kind").toString();
    if (kind == "text") {
        const QString text = p.value("text").toString(), html = p.value("html").toString();
        if ((text.isEmpty() && html.isEmpty()) || text.trimmed().startsWith("LC2.")
            || text.toUtf8().size() + html.toUtf8().size() > MaxContentBytes) return fail(error, "Invalid text payload.");
    } else if (kind == "image") {
        const QByteArray png = decode(p.value("png").toString());
        if (png.isEmpty() || png.size() > MaxContentBytes || !imageSize(png)) return fail(error, "Invalid or oversized PNG.");
    } else if (kind == "files") {
        if (!p.value("files").isArray()) return fail(error, "Invalid file list.");
        const QJsonArray files = p.value("files").toArray();
        if (files.isEmpty() || files.size() > MaxFiles) return fail(error, "Invalid file count.");
        QSet<QString> names;
        qint64 total = 0;
        for (const auto &item : files) {
            const QJsonObject file = item.toObject();
            const QString name = file.value("name").toString();
            if (!safeName(name) || names.contains(name.toCaseFolded()) || !file.value("data").isString()) return fail(error, "Unsafe or duplicate filename.");
            const QString encoded = file.value("data").toString();
            const QByteArray bytes = decode(encoded);
            if ((!encoded.isEmpty() && bytes.isEmpty()) || !file.value("size").isDouble()
                || file.value("size").toDouble() != bytes.size()) return fail(error, "Invalid file data or size.");
            total += bytes.size();
            if (total > MaxContentBytes) return fail(error, "Files exceed 32 MiB.");
            names.insert(name.toCaseFolded());
        }
    } else return fail(error, "Unsupported clipboard format.");
    return true;
}
QMimeData *ClipboardPayload::materialize(const QJsonObject &p, const QString &directory, QString *error) {
    if (!validate(p, error)) return nullptr;
    auto *mime = new QMimeData;
    const QString kind = p.value("kind").toString();
    if (kind == "text") {
        mime->setText(p.value("text").toString());
        if (!p.value("html").toString().isEmpty()) mime->setHtml(p.value("html").toString());
        return mime;
    }
#ifndef Q_OS_ANDROID
    if (kind == "image") {
        const QImage image = QImage::fromData(decode(p.value("png").toString()), "PNG");
        if (image.isNull()) { delete mime; fail(error, "Cannot decode PNG."); return nullptr; }
        mime->setImageData(image);
        return mime;
    }
#endif
    QJsonArray files = p.value("files").toArray();
    if (kind == "image") {
        const QByteArray png = decode(p.value("png").toString());
        files.append(QJsonObject{{"name", "clipboard.png"}, {"data", QString::fromLatin1(png.toBase64())}, {"size", png.size()}});
    }
    const QString folder = QDir(directory).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(folder)) { delete mime; fail(error, "Cannot create received-files directory."); return nullptr; }
    QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QList<QUrl> urls;
    for (const auto &item : files) {
        const QJsonObject obj = item.toObject();
        const QString path = QDir(folder).filePath(obj.value("name").toString());
        QSaveFile file(path);
        const QByteArray bytes = decode(obj.value("data").toString());
        if (!file.open(QIODevice::WriteOnly)) {
            delete mime; QDir(folder).removeRecursively(); fail(error, file.errorString()); return nullptr;
        }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (file.write(bytes) != bytes.size() || !file.commit()) {
            delete mime; QDir(folder).removeRecursively(); fail(error, file.errorString()); return nullptr;
        }
        urls.append(QUrl::fromLocalFile(path));
    }
    mime->setUrls(urls);
#ifdef Q_OS_LINUX
    QByteArray gnome("copy\n");
    for (const QUrl &url : urls) gnome += url.toEncoded() + '\n';
    mime->setData("x-special/gnome-copied-files", gnome);
    mime->setData("application/x-kde-cutselection", "0");
#endif
#ifdef Q_OS_WIN
    mime->setData("application/x-qt-windows-mime;value=\"Preferred DropEffect\"", QByteArray("\x01\x00\x00\x00", 4));
#endif
    return mime;
}
QString ClipboardPayload::hash(const QJsonObject &p) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(p).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
QString ClipboardPayload::description(const QJsonObject &p) {
    const QString kind = p.value("kind").toString();
    if (kind == "files") return QStringLiteral("%1 file(s)").arg(p.value("files").toArray().size());
    if (kind == "image") return QStringLiteral("image");
    return QStringLiteral("text (%1 characters)").arg(p.value("text").toString().size());
}

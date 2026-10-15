#include "AppController.h"
#include "Async.h"
#include "Autostart.h"
#include "I18n.h"
#include "QrCode.h"
#include "QrScanner.h"
#include "FileStorage.h"
#include "ConnectionInfo.h"
#include "ClipboardPayload.h"
#include "Uninstall.h"
#include <QPointer>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif
#ifdef Q_OS_IOS
#include "IOSBridge.h"
#endif

AppController::AppController(const Profile &profile, QObject *parent)
    : QObject(parent), m_session(profile, this) {
    connect(&m_session, &ClipboardSession::profileChanged, this, &AppController::profileChanged);
    connect(&m_session, &ClipboardSession::stateChanged, this, &AppController::stateChanged);
    connect(&m_session, &ClipboardSession::messageChanged, this, &AppController::messageChanged);
    connect(&m_session, &ClipboardSession::busyChanged, this, &AppController::busyChanged);
    connect(&m_session, &ClipboardSession::shareChanged, this, &AppController::shareChanged);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &AppController::stateChanged);
    connect(&I18n::instance(), &I18n::languageChanged, this, [this] {
#ifdef Q_OS_ANDROID
        AndroidBridge::setLanguage(I18n::language());
#endif
        emit languageChanged(); emit messageChanged();
    });
}
QString AppController::message() const { return I18n::text(m_session.message()); }
QString AppController::language() const { return I18n::language(); }
void AppController::setLanguage(const QString &language) { I18n::setLanguage(language); }
bool AppController::registerDevice(const QString &owner, const QString &device, bool join, const QString &code) {
    Profile next = m_session.profile();
    next.ownerName = owner.trimmed(); next.deviceName = device.trimmed(); next.paused = false;
    if (next.ownerName.isEmpty() || next.deviceName.isEmpty() || next.ownerName.size() > 80 || next.deviceName.size() > 80) {
        m_session.report("Enter your name and a device name."); return false;
    }
    if (join) {
        ConnectionInfo info;
        if (!ConnectionInfo::parse(code, &info) || !next.join(info.code)) { m_session.report("Enter the complete connection code from your other device."); return false; }
        next.peerAddresses = info.addresses;
    } else { next.create(); next.peerAddresses.clear(); }
#ifdef Q_OS_IOS
    next.autostart = false;
#endif
    if (!m_session.saveProfile(next)) return false;
    QString error;
    if (next.autostart && !Autostart::setEnabled(true, &error)) m_session.report(error);
    else m_session.report("");
    if (!m_pendingImports.isEmpty()) {
        const auto imports = m_pendingImports; m_pendingImports.clear(); m_session.importFiles(imports);
    }
    return true;
}
void AppController::importFiles(const QList<QUrl> &urls) {
    if (registered()) { m_session.importFiles(urls); return; }
    if (urls.size() > ClipboardPayload::MaxFiles - m_pendingImports.size()) {
        report("Select between 1 and 32 files."); return;
    }
    m_pendingImports.append(urls);
    report("Register this device first.");
}
void AppController::pickFiles() {
    if (!registered() || paused() || busy()) return;
#ifdef Q_OS_IOS
    const QPointer<AppController> self(this);
    IOSBridge::pickFiles(nullptr, [self](const QList<QUrl> &urls, const QString &error) {
        if (!self) return;
        if (!error.isEmpty()) self->report(error); else self->importFiles(urls);
    });
#else
    m_session.sendFiles(QFileDialog::getOpenFileUrls(nullptr, I18n::text("Select files or photos")));
#endif
}
QString AppController::scanQr() { return QrScanner::scan(nullptr); }
QVariantMap AppController::parseConnection(const QString &text) const {
    ConnectionInfo info;
    if (!ConnectionInfo::parse(text, &info)) return {};
    return {{"code", info.code}, {"addresses", info.addresses.join(' ')}, {"invitation", info.encoded()}};
}
QString AppController::localAddresses() const { return ConnectionInfo::forProfile(m_session.profile()).addresses.join(", "); }
void AppController::uninstall() {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    Uninstall::launch(nullptr);
#endif
}
void AppController::quit() { QCoreApplication::quit(); }
void AppController::notificationSettings() {
#ifdef Q_OS_ANDROID
    AndroidBridge::notificationSettings();
#endif
}
QString AppController::invitation() const { return registered() ? m_session.profile().invitation() : QString(); }
QImage AppController::qrImage() const { return registered() ? QrCode::encodeInvitation(ConnectionInfo::forProfile(m_session.profile()).encoded()) : QImage(); }
void AppController::copyInvitation() {
    if (!registered()) return;
    QApplication::clipboard()->setText(invitation());
    m_session.report("Connection code copied. Keep it private.");
}
void AppController::saveQr() {
    if (!registered()) return;
    const QString path = QFileDialog::getSaveFileName(nullptr, I18n::text("Save QR image"), "LocalClipboard-connection.png", "PNG (*.png)");
    if (!path.isEmpty() && !qrImage().save(path, "PNG")) m_session.report("Could not save the QR image.");
}
bool AppController::saveAddresses(const QString &value) {
    QStringList addresses; QString error;
    if (!Profile::parseAddresses(value, &addresses)) {
        m_session.report("Enter up to 32 valid IPv4 addresses."); return false;
    }
    Profile next = m_session.profile(); next.peerAddresses = addresses;
    return m_session.saveProfile(next);
}
void AppController::refreshFiles() {
    if (m_loadingFiles) return;
    m_loadingFiles = true; emit filesChanged();
    const QString root = QDir(Profile::dataDirectory()).filePath("received");
    Async::run(this, [root] {
        QPair<QStringList, QVariantList> result;
        // Enumerate only regular files in our own per-transfer directories.
        const QDir directory(root);
        for (const auto &batch : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Time)) {
            for (const auto &file : QDir(batch.absoluteFilePath()).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Time)) {
                result.first.append(file.absoluteFilePath());
                result.second.append(QVariantMap{{"name", file.fileName()}, {"bytes", file.size()}});
                if (result.first.size() == 500) return result;
            }
        }
        return result;
    }, [this](const QPair<QStringList, QVariantList> &result) {
        m_filePaths = result.first; m_files = result.second;
        m_loadingFiles = false; emit filesChanged();
    });
}
void AppController::openReceivedFile(int index) {
    if (index < 0 || index >= m_filePaths.size()) return;
    const QFileInfo file(m_filePaths.at(index));
    const QString root = QDir(Profile::dataDirectory()).filePath("received");
    if (!FileStorage::isReceivedFile(file.absoluteFilePath(), root)) return;
#ifdef Q_OS_ANDROID
    AndroidBridge::openFile(file.canonicalFilePath());
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(file.canonicalFilePath()));
#endif
}
void AppController::deleteReceivedFile(int index) {
    if (index < 0 || index >= m_filePaths.size()) return;
    const QString path = m_filePaths.at(index);
    if (!FileStorage::isReceivedFile(path, QDir(Profile::dataDirectory()).filePath("received"))) return;
    Async::run(this, [path] { const bool removed = QFile::remove(path);
        if (removed) QDir().rmdir(QFileInfo(path).absolutePath());
        return removed; }, [this](bool removed) {
        if (!removed) m_session.report("Could not delete local file.");
        refreshFiles();
    });
}

#include "MobileController.h"
#include "Async.h"
#include "Autostart.h"
#include "I18n.h"
#include "QrCode.h"
#include "QrScanner.h"
#include "FileStorage.h"
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

MobileController::MobileController(const Profile &profile, QObject *parent)
    : QObject(parent), m_session(profile, this) {
    connect(&m_session, &ClipboardSession::profileChanged, this, &MobileController::profileChanged);
    connect(&m_session, &ClipboardSession::stateChanged, this, &MobileController::stateChanged);
    connect(&m_session, &ClipboardSession::messageChanged, this, &MobileController::messageChanged);
    connect(&m_session, &ClipboardSession::busyChanged, this, &MobileController::busyChanged);
    connect(&m_session, &ClipboardSession::shareChanged, this, &MobileController::shareChanged);
    connect(&I18n::instance(), &I18n::languageChanged, this, [this] {
#ifdef Q_OS_ANDROID
        AndroidBridge::setLanguage(I18n::language());
#endif
        emit languageChanged(); emit messageChanged();
    });
}
QString MobileController::message() const { return I18n::text(m_session.message()); }
QString MobileController::language() const { return I18n::language(); }
void MobileController::setLanguage(const QString &language) { I18n::setLanguage(language); }
bool MobileController::registerDevice(const QString &owner, const QString &device, bool join, const QString &code) {
    Profile next = m_session.profile();
    next.ownerName = owner.trimmed(); next.deviceName = device.trimmed(); next.paused = false;
    if (next.ownerName.isEmpty() || next.deviceName.isEmpty() || next.ownerName.size() > 80 || next.deviceName.size() > 80) {
        m_session.report("Enter your name and a device name."); return false;
    }
    if (join) {
        if (!next.join(code)) { m_session.report("Enter the complete connection code from your other device."); return false; }
    } else next.create();
    if (!m_session.saveProfile(next)) return false;
    QString error;
    if (!Autostart::setEnabled(next.autostart, &error)) m_session.report(error);
    else m_session.report("");
    return true;
}
void MobileController::pickFiles() {
    if (!registered() || paused() || busy()) return;
    m_session.sendFiles(QFileDialog::getOpenFileUrls(nullptr, I18n::text("Select files or photos")));
}
QString MobileController::scanQr() { return QrScanner::scan(nullptr); }
QString MobileController::invitation() const { return registered() ? m_session.profile().invitation() : QString(); }
QImage MobileController::qrImage() const { return QrCode::encodeInvitation(invitation()); }
void MobileController::copyInvitation() {
    if (!registered()) return;
    QApplication::clipboard()->setText(invitation());
    m_session.report("Connection code copied. Keep it private.");
}
void MobileController::saveQr() {
    if (!registered()) return;
    const QString path = QFileDialog::getSaveFileName(nullptr, I18n::text("Save QR image"), "LocalClipboard-connection.png", "PNG (*.png)");
    if (!path.isEmpty() && !qrImage().save(path, "PNG")) m_session.report("Could not save the QR image.");
}
bool MobileController::saveAddresses(const QString &value) {
    QStringList addresses; QString error;
    if (!Profile::parseAddresses(value, &addresses)) {
        m_session.report("Enter up to 32 valid IPv4 addresses."); return false;
    }
    Profile next = m_session.profile(); next.peerAddresses = addresses;
    return m_session.saveProfile(next);
}
void MobileController::refreshFiles() {
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
void MobileController::openReceivedFile(int index) {
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
void MobileController::deleteReceivedFile(int index) {
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

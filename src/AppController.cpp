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
#include "DeviceAccess.h"
#include "LocalHistory.h"
#include "SourceApplication.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QSaveFile>
#include <QSysInfo>
#include <QImageReader>
#include <QLocale>
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
    connect(&m_session, &ClipboardSession::transfersChanged, this, &AppController::transfersChanged);
    connect(&m_session, &ClipboardSession::historyChanged, this, [this] { refreshHistory(); });
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
    next.ownerName = owner.trimmed(); next.deviceName = device.trimmed(); next.paused = false; next.pauseUntil = 0;
    if (next.ownerName.isEmpty() || next.deviceName.isEmpty() || next.ownerName.size() > 80 || next.deviceName.size() > 80) {
        m_session.report("Enter your name and a device name."); return false;
    }
    if (join) {
        ConnectionInfo info;
        if (!ConnectionInfo::parse(code, &info) || !next.join(info.code)) { m_session.report("Enter the complete connection code from your other device."); return false; }
        next.peerAddresses = info.addresses;
    } else { next.createManaged(); next.peerAddresses.clear(); }
#ifdef Q_OS_IOS
    next.autostart = false;
#endif
    if (!m_session.saveProfile(next)) return false;
    LocalHistory::clear(); refreshHistory();
    QString error;
    if (next.autostart && !Autostart::setEnabled(true, &error)) m_session.report(error);
    else if (join && next.managed()) m_session.report("Waiting for approval on the profile owner's device.");
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
                if (!FileStorage::isReceivedFile(file.filePath(), root)) continue;
                result.first.append(file.absoluteFilePath());
                QImageReader reader(file.canonicalFilePath());
                const auto dimensions = reader.size();
                const bool image = reader.canRead() && dimensions.isValid() && qint64(dimensions.width()) * dimensions.height() <= 16000000;
                result.second.append(QVariantMap{{"name", file.fileName()}, {"bytes", file.size()},
                    {"thumbnail", image ? QUrl::fromLocalFile(file.canonicalFilePath()).toString() : QString()},
                    {"size", QLocale().formattedDataSize(file.size())}});
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

QString AppController::accessState() const {
    const auto &p = m_session.profile();
    if (!p.managed() || DeviceAccess::approved(p, p.deviceId, DeviceAccess::publicKey(p))) return "approved";
    for (const auto &entry : p.registry.value("devices").toArray()) {
        const auto d = entry.toObject();
        if (d.value("id").toString() == p.deviceId && d.value("revoked").toBool()) return "revoked";
    }
    return "pending";
}
bool AppController::administrator() const { return DeviceAccess::isAdministrator(m_session.profile()); }
QString AppController::fingerprint() const { return DeviceAccess::fingerprint(DeviceAccess::publicKey(m_session.profile())); }
bool AppController::sourceExclusionsSupported() const { return SourceApplication::supported(); }
bool AppController::authorizeDevice(const QString &id, bool revoke) {
    if (!m_session.authorizeDevice(id, revoke)) { report("Device access could not be changed. Use the profile owner's device."); return false; }
    return true;
}
bool AppController::renameDevice(const QString &name) {
    Profile next = m_session.profile(); next.deviceName = name.trimmed();
    if (!next.registered()) { report("Enter a device name between 1 and 80 characters."); return false; }
    return m_session.saveProfile(next);
}
bool AppController::retryTransfer(const QString &id) {
    if (m_session.retryTransfer(id)) return true;
    report("Retry is unavailable. Connect the device and send again."); return false;
}
void AppController::resendFile(int index) {
    if (index < 0 || index >= m_filePaths.size()) return;
    const auto path = m_filePaths.at(index);
    if (FileStorage::isReceivedFile(path, QDir(Profile::dataDirectory()).filePath("received"))) m_session.sendReceivedFile(QUrl::fromLocalFile(path));
}
void AppController::pauseFor15Minutes() {
    Profile next = m_session.profile(); next.paused = false; next.pauseUntil = QDateTime::currentSecsSinceEpoch() + 900;
    m_session.saveProfile(next);
}
void AppController::setSendingEnabled(bool enabled) { Profile next = m_session.profile(); next.sendingEnabled = enabled; m_session.saveProfile(next); }
void AppController::setReceivingEnabled(bool enabled) { Profile next = m_session.profile(); next.receivingEnabled = enabled; m_session.saveProfile(next); }
void AppController::setHistoryEnabled(bool enabled) {
    Profile next = m_session.profile(); next.historyEnabled = enabled;
    if (m_session.saveProfile(next)) { if (!enabled) LocalHistory::clear(); refreshHistory(); }
}
void AppController::setHistoryLimits(int days, int mib) {
    Profile next = m_session.profile(); next.historyDays = qBound(1, days, 30); next.historyMiB = qBound(1, mib, 64);
    if (m_session.saveProfile(next)) refreshHistory();
}
bool AppController::setExcludedApplications(const QString &applications) {
    if (!sourceExclusionsSupported() || applications.size() > 4096) return false;
    auto entries = applications.split('\n', Qt::SkipEmptyParts);
    if (entries.size() > 64) return false;
    Profile next = m_session.profile(); next.excludedApplications.clear();
    for (const auto &entry : entries) { if (entry.trimmed().size() > 256) return false; next.excludedApplications.append(entry.trimmed()); }
    return m_session.saveProfile(next);
}
void AppController::refreshHistory(const QString &query) {
    const Profile profile = m_session.profile(); const auto revision = ++m_historyRevision;
    Async::run(this, [profile, query] { return LocalHistory::entries(profile, query.left(256)); }, [this, revision](const QVariantList &list) {
        if (revision != m_historyRevision) return;
        m_history = list; emit historyChanged();
    });
}
void AppController::clearHistory() { LocalHistory::clear(); refreshHistory(); }
QString AppController::translateStatus(const QString &text) const { return I18n::text(text); }
void AppController::checkNetwork() {
    m_session.announce();
    int interfaces = 0;
    for (const auto &interface : QNetworkInterface::allInterfaces())
        if (interface.flags().testFlag(QNetworkInterface::IsUp) && !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) ++interfaces;
    // Deliberately construct an allowlist; never serialize Profile or payload objects.
    const QJsonObject report{{"application", "LocalClipboard"}, {"version", QCoreApplication::applicationVersion()},
        {"qt", qVersion()}, {"os", QSysInfo::productType()}, {"osVersion", QSysInfo::productVersion()},
        {"registered", registered()}, {"managedAccess", managedAccess()}, {"receiving", receiving()},
        {"accessState", accessState()}, {"knownDevices", devices().size()},
        {"paused", paused()}, {"sendingEnabled", sendingEnabled()}, {"receivingEnabled", receivingEnabled()},
        {"activeInterfaces", interfaces}, {"onlineDevices", peers().size()}, {"discoveryPort", 45871},
        {"historyEnabled", historyEnabled()}, {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"guidance", "Use the same LAN, allow Local Network access in macOS Privacy & Security and TCP/UDP in the firewall, approve the device on the owner, and keep iOS open. Guest Wi-Fi or VPNs may isolate devices."}};
    m_diagnostics = QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)); emit diagnosticsChanged();
}
void AppController::copyDiagnostics() { checkNetwork(); QApplication::clipboard()->setText(m_diagnostics); }
void AppController::exportDiagnostics() {
    checkNetwork();
    const auto path = QFileDialog::getSaveFileName(nullptr, I18n::text("Save diagnostic report"), "LocalClipboard-diagnostics.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { report("Could not save diagnostic report."); return; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const auto bytes = m_diagnostics.toUtf8();
    if (file.write(bytes) != bytes.size()) report("Could not save diagnostic report.");
}
bool AppController::upgradeAccess() {
    Profile next = m_session.profile(); next.createManaged(); next.paused = false; next.pauseUntil = 0;
    if (!m_session.saveProfile(next)) return false;
    LocalHistory::clear(); refreshHistory(); emit stateChanged(); return true;
}

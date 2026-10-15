#pragma once
#include "ClipboardSession.h"
#include <QImage>
#include <QVariantList>

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY stateChanged)
    Q_PROPERTY(QVariantList transfers READ transfers NOTIFY transfersChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool historyEnabled READ historyEnabled NOTIFY profileChanged)
    Q_PROPERTY(bool sendingEnabled READ sendingEnabled NOTIFY profileChanged)
    Q_PROPERTY(bool receivingEnabled READ receivingEnabled NOTIFY profileChanged)
    Q_PROPERTY(QString accessState READ accessState NOTIFY profileChanged)
    Q_PROPERTY(bool managedAccess READ managedAccess NOTIFY profileChanged)
    Q_PROPERTY(bool administrator READ administrator NOTIFY profileChanged)
    Q_PROPERTY(QString fingerprint READ fingerprint NOTIFY profileChanged)
    Q_PROPERTY(bool sourceExclusionsSupported READ sourceExclusionsSupported CONSTANT)
    Q_PROPERTY(QString excludedApplications READ excludedApplications NOTIFY profileChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY diagnosticsChanged)
    Q_PROPERTY(int historyDays READ historyDays NOTIFY profileChanged)
    Q_PROPERTY(int historyMiB READ historyMiB NOTIFY profileChanged)
    Q_PROPERTY(bool registered READ registered NOTIFY profileChanged)
    Q_PROPERTY(QString ownerName READ ownerName NOTIFY profileChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY profileChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY profileChanged)
    Q_PROPERTY(bool autostart READ autostart NOTIFY profileChanged)
    Q_PROPERTY(QString addresses READ addresses NOTIFY profileChanged)
    Q_PROPERTY(QStringList peers READ peers NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList receivedFiles READ receivedFiles NOTIFY filesChanged)
    Q_PROPERTY(bool loadingFiles READ loadingFiles NOTIFY filesChanged)
    Q_PROPERTY(QString sharedDescription READ sharedDescription NOTIFY shareChanged)
    Q_PROPERTY(bool receiving READ receiving NOTIFY stateChanged)
    Q_PROPERTY(bool pendingClipboard READ pendingClipboard NOTIFY stateChanged)
    Q_PROPERTY(QString localAddresses READ localAddresses NOTIFY stateChanged)
public:
    explicit AppController(const Profile &profile, QObject *parent = nullptr);
    bool registered() const { return m_session.profile().registered(); }
    QString ownerName() const { return m_session.profile().ownerName; }
    QString deviceName() const { return m_session.profile().deviceName; }
    bool paused() const { return m_session.profile().isPaused(); }
    bool autostart() const { return m_session.profile().autostart; }
    QString addresses() const { return m_session.profile().peerAddresses.join(" "); }
    QStringList peers() const { return m_session.peers(); }
    QString message() const;
    void report(const QString &message) { m_session.report(message); }
    bool busy() const { return m_session.busy(); }
    QString language() const;
    void setLanguage(const QString &language);
    QVariantList receivedFiles() const { return m_files; }
    bool loadingFiles() const { return m_loadingFiles; }
    QString sharedDescription() const { return m_session.sharedDescription(); }
    bool receiving() const { return m_session.receiving(); }
    bool pendingClipboard() const { return m_session.pendingClipboard(); }
    Q_INVOKABLE void copyReceivedClipboard() { m_session.copyReceivedClipboard(); }
    QString localAddresses() const;
    void importFiles(const QList<QUrl> &urls);
    QVariantList devices() const { return m_session.devices(); }
    QVariantList transfers() const { return m_session.transfers(); }
    QVariantList history() const { return m_history; }
    bool historyEnabled() const { return m_session.profile().historyEnabled; }
    bool sendingEnabled() const { return m_session.profile().sendingEnabled; }
    bool receivingEnabled() const { return m_session.profile().receivingEnabled; }
    bool managedAccess() const { return m_session.profile().managed(); }
    bool administrator() const;
    QString accessState() const;
    QString fingerprint() const;
    bool sourceExclusionsSupported() const;
    QString excludedApplications() const { return m_session.profile().excludedApplications.join("\n"); }
    QString diagnostics() const { return m_diagnostics; }
    int historyDays() const { return m_session.profile().historyDays; }
    int historyMiB() const { return m_session.profile().historyMiB; }
    Q_INVOKABLE bool authorizeDevice(const QString &id, bool revoke);
    Q_INVOKABLE bool renameDevice(const QString &name);
    Q_INVOKABLE bool retryTransfer(const QString &id);
    Q_INVOKABLE void cancelTransfer(const QString &id) { m_session.cancelTransfer(id); }
    Q_INVOKABLE void resendFile(int index);
    Q_INVOKABLE void pauseFor15Minutes();
    Q_INVOKABLE void setSendingEnabled(bool enabled);
    Q_INVOKABLE void setReceivingEnabled(bool enabled);
    Q_INVOKABLE void setHistoryEnabled(bool enabled);
    Q_INVOKABLE void setHistoryLimits(int days, int mib);
    Q_INVOKABLE bool setExcludedApplications(const QString &applications);
    Q_INVOKABLE void refreshHistory(const QString &query = {});
    Q_INVOKABLE void copyHistory(const QString &id) { m_session.copyHistory(id); }
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE QString translateStatus(const QString &text) const;
    Q_INVOKABLE void checkNetwork();
    Q_INVOKABLE void exportDiagnostics();
    Q_INVOKABLE void copyDiagnostics();
    Q_INVOKABLE bool upgradeAccess();
    Q_INVOKABLE void finishShare(bool send) { m_session.finishShare(send); }
    QImage qrImage() const;
    Q_INVOKABLE bool registerDevice(const QString &owner, const QString &device, bool join, const QString &code);
    Q_INVOKABLE void sendClipboard() { m_session.sendClipboard(); }
    Q_INVOKABLE void pickFiles();
    Q_INVOKABLE QString scanQr();
    Q_INVOKABLE QVariantMap parseConnection(const QString &text) const;
    Q_INVOKABLE void uninstall();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void notificationSettings();
    Q_INVOKABLE QString invitation() const;
    Q_INVOKABLE void copyInvitation();
    Q_INVOKABLE void saveQr();
    Q_INVOKABLE void setPaused(bool paused) { m_session.setPaused(paused); }
    Q_INVOKABLE void setAutostart(bool enabled) { m_session.setAutostart(enabled); }
    Q_INVOKABLE bool saveAddresses(const QString &addresses);
    Q_INVOKABLE void refreshFiles();
    Q_INVOKABLE void openReceivedFile(int index);
    Q_INVOKABLE void deleteReceivedFile(int index);
signals:
    void profileChanged();
    void stateChanged();
    void messageChanged();
    void busyChanged();
    void languageChanged();
    void filesChanged();
    void shareChanged();
    void transfersChanged();
    void historyChanged();
    void diagnosticsChanged();
private:
    ClipboardSession m_session;
    QVariantList m_files;
    QStringList m_filePaths;
    bool m_loadingFiles = false;
    QList<QUrl> m_pendingImports;
    QVariantList m_history;
    QString m_diagnostics;
    quint64 m_historyRevision = 0;
};

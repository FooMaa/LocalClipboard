#pragma once
#include "ClipboardSession.h"
#include <QImage>
#include <QVariantList>

class AppController final : public QObject {
    Q_OBJECT
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
    bool paused() const { return m_session.profile().paused; }
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
private:
    ClipboardSession m_session;
    QVariantList m_files;
    QStringList m_filePaths;
    bool m_loadingFiles = false;
    QList<QUrl> m_pendingImports;
};

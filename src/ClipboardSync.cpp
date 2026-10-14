#include "ClipboardSync.h"
#include "Autostart.h"
#include "ClipboardPayload.h"
#include "Registration.h"
#include "SyncEngine.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif

ClipboardSync::ClipboardSync(const Profile &profile, QWidget *parent) : QMainWindow(parent), m_profile(profile) {
    setWindowTitle("LocalClipboard — " + m_profile.ownerName);
    resize(600, 460);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *intro = new QLabel("Your profile: " + profile.ownerName + " · Device: " + profile.deviceName +
        "\nOnly devices registered with your connection code synchronize. Text, images and files stay on your LAN.", central);
    intro->setWordWrap(true); layout->addWidget(intro);
#ifdef Q_OS_ANDROID
    auto *androidNote = new QLabel("Background reception is enabled. Tap a received-item notification to apply it. "
        "Android permits clipboard reading only while this app has focus. You can also send photos or files with Share → LocalClipboard.", central);
    androidNote->setWordWrap(true); layout->addWidget(androidNote);
#endif
    m_peers = new QListWidget(central); layout->addWidget(m_peers, 1);
    m_pause = new QCheckBox("Pause synchronization", central); m_pause->setChecked(profile.paused); layout->addWidget(m_pause);
    auto *startup = new QCheckBox("Start in background at login / device restart", central);
    startup->setChecked(profile.autostart); layout->addWidget(startup);
    auto *send = new QPushButton("Send current clipboard", central);
    auto *files = new QPushButton("Send files / photos…", central);
    auto *pair = new QPushButton("Connect another device…", central);
    auto *change = new QPushButton("Change personal profile…", central);
    auto *received = new QPushButton("Open received files", central);
    auto *quit = new QPushButton("Quit LocalClipboard", central);
    layout->addWidget(send); layout->addWidget(files); layout->addWidget(pair); layout->addWidget(change); layout->addWidget(received);
    layout->addWidget(quit);
    m_status = new QLabel(central); layout->addWidget(m_status); setCentralWidget(central);
    connect(send, &QPushButton::clicked, this, [this] { sendClipboard(true); });
    connect(quit, &QPushButton::clicked, this, [] {
#ifdef Q_OS_ANDROID
        AndroidBridge::stopService();
#endif
        QCoreApplication::quit();
    });
    connect(files, &QPushButton::clicked, this, [this] {
        const QList<QUrl> urls = QFileDialog::getOpenFileUrls(this, "Select files or photos");
        if (urls.isEmpty()) return;
        QString error;
        const QJsonObject payload = ClipboardPayload::fromFiles(urls, &error);
        if (!payload.isEmpty()) sendPayload(payload); else log(error);
    });
    connect(pair, &QPushButton::clicked, this, [this] { showInvitation(m_profile, this); });
    connect(change, &QPushButton::clicked, this, [this, intro, startup] {
        Profile next = m_profile;
        if (!registerDevice(&next, this)) return;
        m_profile = next; m_lastHash.clear(); m_pause->setChecked(false); startup->setChecked(next.autostart);
        setWindowTitle("LocalClipboard — " + next.ownerName);
        intro->setText("Your profile: " + next.ownerName + " · Device: " + next.deviceName +
            "\nOnly devices registered with your connection code synchronize. Text, images and files stay on your LAN.");
        QString error;
        if (!Autostart::setEnabled(next.autostart, &error)) log(error);
        startEngine();
    });
    connect(received, &QPushButton::clicked, this, [this] {
        const QString folder = QDir(Profile::dataDirectory()).filePath("received");
        QDir().mkpath(folder);
#ifdef Q_OS_ANDROID
        AndroidBridge::openReceivedFiles(this);
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
    });
    connect(m_pause, &QCheckBox::toggled, this, [this](bool paused) {
        m_profile.paused = paused; QString error;
        if (!m_profile.save(&error)) log(error);
        if (m_engine) m_engine->setPaused(paused);
        refresh();
    });
    connect(startup, &QCheckBox::toggled, this, [this, startup](bool enabled) {
        QString error;
        if (!Autostart::setEnabled(enabled, &error)) {
            startup->blockSignals(true); startup->setChecked(m_profile.autostart); startup->blockSignals(false);
            log("Autostart failed: " + error); return;
        }
        m_profile.autostart = enabled;
        if (!m_profile.save(&error)) log(error);
    });
#ifndef Q_OS_ANDROID
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] { sendClipboard(); });
    QString ignored;
    const auto initial = ClipboardPayload::capture(QApplication::clipboard()->mimeData(), &ignored);
    if (!initial.isEmpty()) m_lastHash = ClipboardPayload::hash(initial);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new QSystemTrayIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView), this);
        m_tray->setToolTip("LocalClipboard — " + profile.ownerName);
        auto *menu = new QMenu(this);
        menu->addAction("Open LocalClipboard", this, &ClipboardSync::showSettings);
        auto *pause = menu->addAction("Pause synchronization"); pause->setCheckable(true); pause->setChecked(profile.paused);
        connect(pause, &QAction::toggled, m_pause, &QCheckBox::setChecked);
        connect(m_pause, &QCheckBox::toggled, pause, &QAction::setChecked);
        menu->addSeparator(); menu->addAction("Quit", qApp, &QCoreApplication::quit);
        m_tray->setContextMenu(menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) showSettings();
        });
        m_tray->show();
    }
#else
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) AndroidBridge::consumeInbox(m_profile, [this](const QJsonObject &p) { apply(p); });
    });
    auto *poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] {
        refresh();
        if (QApplication::applicationState() != Qt::ApplicationActive) return;
        if (!m_pause->isChecked()) {
            AndroidBridge::consumeInbox(m_profile, [this](const QJsonObject &p) { apply(p); });
            AndroidBridge::consumeShares([this](const QJsonObject &p) { sendPayload(p); }, [this](const QString &e) { log(e); });
        }
    });
    poll->start(1000);
#endif
    startEngine();
}
void ClipboardSync::startEngine() {
    delete m_engine; m_engine = nullptr;
#ifdef Q_OS_ANDROID
    AndroidBridge::startService(m_profile.autostart);
#else
    m_engine = new SyncEngine(m_profile, this);
    connect(m_engine, &SyncEngine::received, this, &ClipboardSync::apply);
    connect(m_engine, &SyncEngine::message, this, &ClipboardSync::log);
    connect(m_engine, &SyncEngine::peersChanged, this, &ClipboardSync::refresh);
#endif
    refresh();
}
void ClipboardSync::sendClipboard(bool explicitSend) {
    if (m_applying || m_pause->isChecked()) return;
#ifdef Q_OS_ANDROID
    if (QApplication::applicationState() != Qt::ApplicationActive) return;
#endif
    QString error;
    const QJsonObject payload = ClipboardPayload::capture(QApplication::clipboard()->mimeData(), &error);
    if (payload.isEmpty()) { m_lastHash.clear(); if (!error.isEmpty()) log(error); return; }
    const QString hash = ClipboardPayload::hash(payload);
    if (!explicitSend && hash == m_lastHash) return;
    m_lastHash = hash;
    sendPayload(payload);
}
void ClipboardSync::sendPayload(const QJsonObject &payload) {
    if (m_pause->isChecked()) return;
#ifdef Q_OS_ANDROID
    QString error;
    if (!AndroidBridge::queueOutgoing(m_profile, payload, &error)) log(error);
    else log("Queued " + ClipboardPayload::description(payload));
#else
    if (!m_engine->send(payload)) log("No registered device is online, or synchronization is paused.");
#endif
}
void ClipboardSync::apply(const QJsonObject &payload) {
    if (m_pause->isChecked()) return;
    const QString hash = ClipboardPayload::hash(payload);
#ifndef Q_OS_ANDROID
    if (hash == m_lastHash) return;
#endif
    QString error;
    QMimeData *mime = ClipboardPayload::materialize(payload, QDir(Profile::dataDirectory()).filePath("received"), &error);
    if (!mime) { log(error); return; }
    m_applying = true;
    m_lastHash = hash;
#ifdef Q_OS_ANDROID
    if (mime->hasUrls()) {
        AndroidBridge::copyFiles(mime->urls()); delete mime;
    } else QApplication::clipboard()->setMimeData(mime);
#else
    QApplication::clipboard()->setMimeData(mime);
    const auto normalized = ClipboardPayload::capture(QApplication::clipboard()->mimeData(), &error);
    if (!normalized.isEmpty()) m_lastHash = ClipboardPayload::hash(normalized);
#endif
    m_applying = false;
    log("Applied " + ClipboardPayload::description(payload));
}
void ClipboardSync::refresh() {
    m_peers->clear();
#ifdef Q_OS_ANDROID
    m_peers->addItems(AndroidBridge::peerDescriptions(m_profile));
#else
    if (m_engine) m_peers->addItems(m_engine->peerDescriptions());
#endif
    m_status->setText(QStringLiteral("%1 registered device(s) online · %2").arg(m_peers->count())
        .arg(m_pause->isChecked() ? "paused" : "active"));
}
void ClipboardSync::log(const QString &message) { statusBar()->showMessage(message, 15000); }
bool ClipboardSync::hasTray() const { return m_tray && m_tray->isVisible(); }
void ClipboardSync::showSettings() { showNormal(); raise(); activateWindow(); }
void ClipboardSync::closeEvent(QCloseEvent *event) {
#ifdef Q_OS_ANDROID
    QMainWindow::closeEvent(event);
#else
    hide(); event->ignore();
#endif
}

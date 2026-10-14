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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPointer>
#include <QScrollArea>
#include <QRegularExpression>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include <QSystemTrayIcon>
#endif
#include <QTimer>
#include <QVBoxLayout>
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif
#ifdef Q_OS_IOS
#include "IOSBridge.h"
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
#ifdef Q_OS_IOS
    auto *iosNote = new QLabel("Keep this app open to receive. Tap Send current clipboard to send text or images. "
        "Use Files / Photos, or Open in LocalClipboard from another app, to send files. "
        "Add your computer's local IP in Network settings and allow Local Network access.", central);
    iosNote->setWordWrap(true); layout->addWidget(iosNote);
#endif
    m_peers = new QListWidget(central); layout->addWidget(m_peers, 1);
    m_pause = new QCheckBox("Pause synchronization", central); m_pause->setChecked(profile.paused); layout->addWidget(m_pause);
    auto *startup = new QCheckBox("Start in background at login / device restart", central);
    startup->setChecked(profile.autostart); layout->addWidget(startup);
    auto *network = new QPushButton("Network settings…", central); layout->addWidget(network);
    connect(network, &QPushButton::clicked, this, &ClipboardSync::networkSettings);
    auto *send = new QPushButton("Send current clipboard", central);
    auto *files = new QPushButton("Send files / photos…", central);
    auto *pair = new QPushButton("Connect another device…", central);
    auto *change = new QPushButton("Change personal profile…", central);
    auto *received = new QPushButton("Open received files", central);
    auto *quit = new QPushButton("Quit LocalClipboard", central);
    layout->addWidget(send); layout->addWidget(files); layout->addWidget(pair); layout->addWidget(change); layout->addWidget(received);
    layout->addWidget(quit);
    m_status = new QLabel(central); layout->addWidget(m_status); setCentralWidget(central);
#ifdef Q_OS_IOS
    startup->hide(); quit->hide();
    // Phone screens must remain usable with larger accessibility text sizes.
    takeCentralWidget();
    auto *scroll = new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setWidget(central);
    setCentralWidget(scroll);
#endif
    connect(send, &QPushButton::clicked, this, [this] { sendClipboard(true); });
    connect(quit, &QPushButton::clicked, this, [] {
#ifdef Q_OS_ANDROID
        AndroidBridge::stopService();
#endif
        QCoreApplication::quit();
    });
    connect(files, &QPushButton::clicked, this, [this] {
#ifdef Q_OS_IOS
        const QPointer<ClipboardSync> self(this);
        IOSBridge::pickFiles(this, [self](const QList<QUrl> &urls, const QString &error) {
            if (!self) return;
            if (!error.isEmpty()) { self->log(error); return; }
            if (urls.isEmpty()) return;
            QString message;
            if (!IOSBridge::queueFiles(self->m_profile, urls, &message)) self->log(message);
            else self->log("Files saved for sending when a registered device is online.");
        });
#else
        const QList<QUrl> urls = QFileDialog::getOpenFileUrls(this, "Select files or photos");
        if (urls.isEmpty()) return;
        QString error;
        const QJsonObject payload = ClipboardPayload::fromFiles(urls, &error);
        if (!payload.isEmpty()) sendPayload(payload); else log(error);
#endif
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
#elif defined(Q_OS_IOS)
        IOSBridge::openReceivedFiles(this);
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
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
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
#elif defined(Q_OS_ANDROID)
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
#else
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        // iOS suspends ordinary apps. Tear down sockets so desktops stop listing us.
        if (state == Qt::ApplicationActive) startEngine();
        else { delete m_engine; m_engine = nullptr; refresh(); }
    });
    auto *poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] {
        if (!m_engine || m_profile.paused || m_engine->peerDescriptions().isEmpty()) return;
        QString error;
        const auto urls = IOSBridge::queuedFiles(m_profile);
        if (urls.isEmpty()) return;
        const auto payload = ClipboardPayload::fromFiles(urls, &error);
        if (payload.isEmpty()) { log(error); return; }
        if (m_engine->send(payload)) {
            IOSBridge::clearQueue(m_profile);
            log("Sending imported files. Copies remain under Imported files for retry.");
        }
    });
    poll->start(1000);
#endif
    startEngine();
}
void ClipboardSync::startEngine() {
    delete m_engine; m_engine = nullptr;
#ifdef Q_OS_IOS
    if (QApplication::applicationState() != Qt::ApplicationActive) { refresh(); return; }
#endif
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
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
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
    if (!m_engine || !m_engine->send(payload)) log("No registered device is online, or synchronization is paused.");
#endif
}
void ClipboardSync::apply(const QJsonObject &payload) {
    if (m_pause->isChecked()) return;
    const QString hash = ClipboardPayload::hash(payload);
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
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
#elif defined(Q_OS_IOS)
    if (mime->hasUrls()) { IOSBridge::copyFiles(mime->urls()); delete mime; }
    else QApplication::clipboard()->setMimeData(mime);
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
#ifdef Q_OS_IOS
    if (!m_engine) m_status->setText("Reception resumes when LocalClipboard is open.");
#endif
}
void ClipboardSync::log(const QString &message) { statusBar()->showMessage(message, 15000); }
bool ClipboardSync::hasTray() const {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return false;
#else
    return m_tray && m_tray->isVisible();
#endif
}
void ClipboardSync::showSettings() { showNormal(); raise(); activateWindow(); }
void ClipboardSync::closeEvent(QCloseEvent *event) {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QMainWindow::closeEvent(event);
#else
    hide(); event->ignore();
#endif
}

void ClipboardSync::networkSettings() {
    QDialog dialog(this); dialog.setWindowTitle("Network settings");
    auto *form = new QFormLayout(&dialog);
    QStringList local;
    for (const auto &address : QNetworkInterface::allAddresses())
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) local.append(address.toString());
    auto *help = new QLabel("This device: " + local.join(", ") + "\nEnter a registered device's local IPv4 address. "
        "Both devices must be on the same Wi-Fi/LAN. The connection code is still required. "
        "Separate multiple addresses with spaces. Update addresses here if your router changes them.", &dialog);
    help->setWordWrap(true); form->addRow(help);
    auto *addresses = new QLineEdit(m_profile.peerAddresses.join(" "), &dialog);
    addresses->setPlaceholderText("192.168.1.10"); form->addRow("Device IP addresses", addresses);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QStringList peers;
        for (const auto &value : addresses->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts)) {
            const QHostAddress address(value);
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isNull()
                || address.isMulticast() || address == QHostAddress::Broadcast || peers.size() >= 32) {
                QMessageBox::warning(&dialog, "Network", "Enter up to 32 valid IPv4 addresses."); return;
            }
            if (!peers.contains(address.toString())) peers.append(address.toString());
        }
        Profile next = m_profile; next.peerAddresses = peers;
        QString error;
        if (!next.save(&error)) { QMessageBox::warning(&dialog, "Network", error); return; }
        m_profile = next; startEngine(); dialog.accept();
    });
    dialog.exec();
}
void ClipboardSync::importFiles(const QList<QUrl> &urls) {
#ifdef Q_OS_IOS
    // IOSApplication already copied the security-scoped URLs during FileOpen.
    QString message;
    if (!IOSBridge::queueFiles(m_profile, urls, &message)) log(message);
    else { showSettings(); log("Imported files queued for your registered devices."); }
#else
    Q_UNUSED(urls);
#endif
}

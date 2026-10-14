#include "ClipboardSync.h"
#include "Autostart.h"
#include "ClipboardPayload.h"
#include "Registration.h"
#include "I18n.h"
#include "Ui.h"
#include "Uninstall.h"
#include <QComboBox>
#include <QHBoxLayout>
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
    Ui::prepareWindow(this);
    setWindowTitle("LocalClipboard — " + m_profile.ownerName);
    if (!Ui::touchMode()) resize(580, 760);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(12);
    layout->addWidget(Ui::languagePicker(central), 0, Qt::AlignRight);
    layout->addWidget(Ui::label("LocalClipboard", central, "heading"));
    auto *intro = Ui::label("", central);
    const auto updateProfile = [this, intro] {
        intro->setText(I18n::text("Your profile: %1 · Device: %2").arg(m_profile.ownerName, m_profile.deviceName));
        setWindowTitle("LocalClipboard — " + m_profile.ownerName);
    };
    updateProfile(); connect(&I18n::instance(), &I18n::languageChanged, this, updateProfile);
    layout->addWidget(intro);
    layout->addWidget(Ui::label("Only devices registered with your connection code synchronize. Text, images and files stay on your LAN.", central, "muted"));
#ifdef Q_OS_ANDROID
    layout->addWidget(Ui::label("Background reception is enabled. Tap a received-item notification to apply it. Android permits clipboard reading only while this app has focus. You can also send photos or files with Share → LocalClipboard.", central, "muted"));
#endif
#ifdef Q_OS_IOS
    layout->addWidget(Ui::label("Keep this app open to receive. Tap Send current clipboard to send text or images. Use Files / Photos, or Open in LocalClipboard from another app, to send files. Add your computer's local IP in Network settings and allow Local Network access.", central, "muted"));
#endif
    layout->addWidget(Ui::label("Your devices", central, "section"));
    m_peers = new QListWidget(central); m_peers->setMinimumHeight(100); m_peers->setMaximumHeight(160);
    m_peers->setWordWrap(true); layout->addWidget(m_peers);
    m_empty = Ui::label("No devices online yet. Open LocalClipboard on another device with the same profile.", central, "muted"); layout->addWidget(m_empty);
    m_status = Ui::label("", central, "section"); layout->addWidget(m_status);
    m_log = Ui::label("", central, "muted"); layout->addWidget(m_log);
    layout->addWidget(Ui::label("Send", central, "section"));
    auto *send = Ui::button("Send clipboard", central, true);
    auto *files = Ui::button("Send files / photos…", central);
    auto *received = Ui::button("Received files", central);
    layout->addWidget(send); layout->addWidget(files); layout->addWidget(received);
    layout->addWidget(Ui::label("Settings", central, "section"));
    m_pause = Ui::checkBox("Pause synchronization", central); m_pause->setChecked(profile.paused); layout->addWidget(m_pause);
    auto *startup = Ui::checkBox("Start automatically", central); startup->setChecked(profile.autostart);
#ifndef Q_OS_IOS
    layout->addWidget(startup);
#endif
    auto *network = Ui::button("Network settings…", central); layout->addWidget(network);
    connect(network, &QPushButton::clicked, this, &ClipboardSync::networkSettings);
    auto *pair = Ui::button("Connect device…", central);
    auto *change = Ui::button("Change profile…", central);
    auto *quit = Ui::button("Quit LocalClipboard", central);
#ifdef Q_OS_IOS
    startup->hide(); quit->hide();
#endif
    layout->addWidget(pair); layout->addWidget(change);
#ifndef Q_OS_IOS
    layout->addWidget(quit);
#endif
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    auto *uninstall = Ui::button("Uninstall…", central); layout->addWidget(uninstall);
    connect(uninstall, &QPushButton::clicked, this, [this] { Uninstall::launch(this); });
#endif
    layout->addStretch();
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    auto *scroll = new QScrollArea(this); scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setWidget(central); setCentralWidget(scroll); Ui::enableTouchScroll(scroll);
    connect(&I18n::instance(), &I18n::languageChanged, this, &ClipboardSync::refresh);
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
        const QList<QUrl> urls = QFileDialog::getOpenFileUrls(this, I18n::text("Select files or photos"));
        if (urls.isEmpty()) return;
        QString error;
        const QJsonObject payload = ClipboardPayload::fromFiles(urls, &error);
        if (!payload.isEmpty()) sendPayload(payload); else log(error);
#endif
    });
    connect(pair, &QPushButton::clicked, this, [this] { showInvitation(m_profile, this); });
    connect(change, &QPushButton::clicked, this, [this, startup, updateProfile] {
        Profile next = m_profile;
        if (!registerDevice(&next, this)) return;
        m_profile = next; m_lastHash.clear(); m_pause->setChecked(false); startup->setChecked(next.autostart);
        updateProfile();
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
        QIcon trayIcon = QApplication::windowIcon();
#ifdef Q_OS_MACOS
        trayIcon = QIcon();
        trayIcon.addFile(":/icons/localclipboard-tray-32.png");
        trayIcon.addFile(":/icons/localclipboard-tray-64.png");
        trayIcon.setIsMask(true);
#endif
        m_tray = new QSystemTrayIcon(trayIcon, this);
        m_tray->setToolTip("LocalClipboard — " + profile.ownerName);
        auto *menu = new QMenu(this);
        auto *open = menu->addAction(I18n::text("Open LocalClipboard"), this, &ClipboardSync::showSettings);
        auto *pause = menu->addAction(I18n::text("Pause synchronization")); pause->setCheckable(true); pause->setChecked(profile.paused);
        connect(pause, &QAction::toggled, m_pause, &QCheckBox::setChecked);
        connect(m_pause, &QCheckBox::toggled, pause, &QAction::setChecked);
        menu->addSeparator(); auto *exit = menu->addAction(I18n::text("Quit"), qApp, &QCoreApplication::quit);
        connect(&I18n::instance(), &I18n::languageChanged, menu, [open, pause, exit] {
            open->setText(I18n::text("Open LocalClipboard")); pause->setText(I18n::text("Pause synchronization")); exit->setText(I18n::text("Quit"));
        });
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
    m_empty->setVisible(m_peers->count() == 0);
    m_status->setText(I18n::text("%1 registered device(s) online · %2").arg(m_peers->count())
        .arg(I18n::text(m_pause->isChecked() ? "paused" : "active")));
#ifdef Q_OS_IOS
    if (!m_engine) m_status->setText(I18n::text("Reception resumes when LocalClipboard is open."));
#endif
}
void ClipboardSync::log(const QString &message) { m_log->setText(I18n::text(message)); }
bool ClipboardSync::hasTray() const {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return false;
#else
    return m_tray && m_tray->isVisible();
#endif
}
void ClipboardSync::showSettings() {
    if (Ui::touchMode()) Ui::showWindow(this); else showNormal();
    raise(); activateWindow();
}
void ClipboardSync::closeEvent(QCloseEvent *event) {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QMainWindow::closeEvent(event);
#else
    hide(); event->ignore();
#endif
}

void ClipboardSync::networkSettings() {
    QDialog dialog(this); Ui::title(&dialog, "Network settings");
    const auto form = Ui::form(&dialog, "Network settings", "Enter a registered device's local IPv4 address. Both devices must be on the same Wi-Fi/LAN. Separate multiple addresses with spaces. Update them if your router changes them.");
    QStringList local;
    for (const auto &address : QNetworkInterface::allAddresses())
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) local.append(address.toString());
    auto *help = Ui::label("", &dialog, "muted");
    const auto update = [help, local] { help->setText(I18n::text("This device: %1").arg(local.join(", "))); };
    update(); connect(&I18n::instance(), &I18n::languageChanged, &dialog, update); form.body->addWidget(help);
    auto *addresses = new QLineEdit(m_profile.peerAddresses.join(" "), &dialog);
    addresses->setPlaceholderText("192.168.1.10"); Ui::field(form.body, "Device IP addresses", addresses); form.body->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form.footer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QStringList peers;
        for (const auto &value : addresses->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts)) {
            const QHostAddress address(value);
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isNull()
                || address.isMulticast() || address == QHostAddress::Broadcast || peers.size() >= 32) {
                QMessageBox::warning(&dialog, I18n::text("Network"), I18n::text("Enter up to 32 valid IPv4 addresses.")); return;
            }
            if (!peers.contains(address.toString())) peers.append(address.toString());
        }
        Profile next = m_profile; next.peerAddresses = peers;
        QString error;
        if (!next.save(&error)) { QMessageBox::warning(&dialog, I18n::text("Network"), I18n::text(error)); return; }
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

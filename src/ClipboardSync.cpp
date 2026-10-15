#include "ClipboardSync.h"
#include "ClipboardSession.h"
#include <QSignalBlocker>
#include "Autostart.h"
#include "Registration.h"
#include "I18n.h"
#include "Ui.h"
#include "Uninstall.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPointer>
#include <QScrollArea>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include <QSystemTrayIcon>
#endif
#include <QVBoxLayout>
#ifdef Q_OS_ANDROID
#include "AndroidBridge.h"
#endif
#ifdef Q_OS_IOS
#include "IOSBridge.h"
#endif

ClipboardSync::ClipboardSync(const Profile &profile, QWidget *parent) : QMainWindow(parent), m_session(new ClipboardSession(profile, this)) {
    Ui::prepareWindow(this);
    setWindowTitle("LocalClipboard — " + m_session->profile().ownerName);
    if (!Ui::touchMode()) resize(580, 760);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(12);
    layout->addWidget(Ui::languagePicker(central), 0, Qt::AlignRight);
    layout->addWidget(Ui::label("LocalClipboard", central, "heading"));
    auto *intro = Ui::label("", central);
    const auto updateProfile = [this, intro] {
        intro->setText(I18n::text("Your profile: %1 · Device: %2").arg(m_session->profile().ownerName, m_session->profile().deviceName));
        setWindowTitle("LocalClipboard — " + m_session->profile().ownerName);
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
    connect(send, &QPushButton::clicked, this, [this] { m_session->sendClipboard(true); });
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
            if (!error.isEmpty()) self->m_session->report(error);
            else self->m_session->importFiles(urls);
        });
#else
        m_session->sendFiles(QFileDialog::getOpenFileUrls(this, I18n::text("Select files or photos")));
#endif
    });
    connect(pair, &QPushButton::clicked, this, [this] { showInvitation(m_session->profile(), this); });
    connect(change, &QPushButton::clicked, this, [this, startup, updateProfile] {
        Profile next = m_session->profile();
        if (!registerDevice(&next, this)) return;
        m_session->replaceProfile(next);
        updateProfile();
        QString error;
        if (!Autostart::setEnabled(next.autostart, &error)) m_session->report(error);
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
    connect(m_pause, &QCheckBox::toggled, m_session, &ClipboardSession::setPaused);
    connect(startup, &QCheckBox::toggled, this, [this, startup](bool enabled) {
        m_session->setAutostart(enabled);
        const QSignalBlocker blocker(startup);
        startup->setChecked(m_session->profile().autostart);
    });
    connect(m_session, &ClipboardSession::profileChanged, this, [this, startup, updateProfile] {
        const QSignalBlocker pausedBlock(m_pause), startupBlock(startup);
        m_pause->setChecked(m_session->profile().paused);
        startup->setChecked(m_session->profile().autostart);
        updateProfile(); refresh();
    });
    connect(m_session, &ClipboardSession::stateChanged, this, &ClipboardSync::refresh);
    connect(m_session, &ClipboardSession::messageChanged, this, [this] { m_log->setText(I18n::text(m_session->message())); });
    connect(m_session, &ClipboardSession::busyChanged, this, [this, send, files] {
        send->setEnabled(!m_session->busy()); files->setEnabled(!m_session->busy());
    });
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
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
#endif
    refresh();
}
void ClipboardSync::refresh() {
    const QStringList peers = m_session->peers();
    if (peers != m_visiblePeers) {
        m_visiblePeers = peers; m_peers->clear(); m_peers->addItems(peers);
    }
    m_empty->setVisible(m_peers->count() == 0);
    m_status->setText(I18n::text("%1 registered device(s) online · %2").arg(m_peers->count())
        .arg(I18n::text(m_pause->isChecked() ? "paused" : "active")));
#ifdef Q_OS_IOS
    if (!m_session->receiving()) m_status->setText(I18n::text("Reception resumes when LocalClipboard is open."));
#endif
}
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
    auto *addresses = new QLineEdit(m_session->profile().peerAddresses.join(" "), &dialog);
    addresses->setPlaceholderText("192.168.1.10"); Ui::field(form.body, "Device IP addresses", addresses); form.body->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form.footer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QStringList peers;
        if (!Profile::parseAddresses(addresses->text(), &peers)) {
            QMessageBox::warning(&dialog, I18n::text("Network"), I18n::text("Enter up to 32 valid IPv4 addresses.")); return;
        }
        Profile next = m_session->profile(); next.peerAddresses = peers;
        if (m_session->saveProfile(next)) dialog.accept();
    });
    dialog.exec();
}
void ClipboardSync::importFiles(const QList<QUrl> &urls) { m_session->importFiles(urls); }

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme.js" as Theme

ApplicationWindow {
    id: window
    required property QtObject controller
    property string platformName: Qt.platform.os
    readonly property bool phone: platformName === "android" || platformName === "ios"
    readonly property bool wide: !phone && width >= 820
    property bool startHidden: false
    property real safeTop: 0
    property real safeBottom: 0
    width: phone ? 412 : 1000
    height: phone ? 860 : 780
    minimumWidth: phone ? 0 : 360
    minimumHeight: phone ? 0 : 440
    visible: !startHidden
    title: "LocalClipboard"
    color: Theme.background
    font.pixelSize: 16
    property string page: controller.registered ? "home" : "register"
    property bool changingProfile: false
    property int qrRevision: 0
    readonly property int pendingApprovals: {
        if (!controller.administrator) return 0
        let count = 0
        const devices = controller.devices
        for (let i = 0; i < devices.length; ++i)
            if (!devices[i].approved && !devices[i].revoked) ++count
        return count
    }
    // ApplicationWindow creates this internal Control in C++, outside QML's
    // componentComplete path. Initialize its accessibility attachment here too.
    Component.onCompleted: contentItem.parent.Accessible.name = title
    function showPairing() { qrRevision++; page = "pair" }
    function navigate(destination) {
        page = destination
        if (destination === "files") controller.refreshFiles()
        if (destination === "history") controller.refreshHistory()
        if (destination === "diagnostics") controller.checkNetwork()
    }
    function back() {
        if (page === "register" && !controller.registered) return false
        if (page !== "home") { page = "home"; return true }
        return false
    }
    onClosing: function(close) { if (phone && back()) close.accepted = false }
    Shortcut { sequence: "Ctrl+1"; enabled: controller.registered; onActivated: window.page = "home" }
    Shortcut { sequence: "Ctrl+2"; enabled: controller.registered; onActivated: window.navigate("files") }
    Shortcut { sequence: "Ctrl+,"; enabled: controller.registered; onActivated: window.page = "settings" }
    Shortcut { sequence: "Escape"; onActivated: window.back() }
    Connections {
        target: controller
        function onMessageChanged() { if (window.page === "files") controller.refreshFiles() }
    }
    Dialog {
        id: shareConfirmation
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 460)
        modal: true
        closePolicy: Popup.NoAutoClose
        visible: controller.sharedDescription.length > 0
        padding: 24
        background: Rectangle { color: Theme.surface; radius: 24; border.color: Theme.line }
        contentItem: Column {
            spacing: 20
            BodyText { width: parent.width; text: qsTr("Send shared content?"); font.pixelSize: 22; font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: qsTr("Another app supplied %1. Send it to your registered devices?").arg(controller.sharedDescription); color: Theme.muted }
            ActionButton { width: parent.width; primary: true; text: qsTr("Send"); enabled: !controller.paused; onClicked: controller.finishShare(true) }
            ActionButton { width: parent.width; text: qsTr("Cancel"); onClicked: controller.finishShare(false) }
        }
    }
    Dialog {
        id: deleteConfirmation
        property int fileIndex: -1
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 460)
        modal: true
        padding: 24
        background: Rectangle { color: Theme.surface; radius: 24; border.color: Theme.line }
        contentItem: Column {
            spacing: 20
            BodyText { width: parent.width; text: qsTr("Delete the local copy?"); font.pixelSize: 22; font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: qsTr("Copies on your other devices will be kept."); color: Theme.muted }
            ActionButton { width: parent.width; text: qsTr("Delete"); primary: true; onClicked: { controller.deleteReceivedFile(deleteConfirmation.fileIndex); deleteConfirmation.close() } }
            ActionButton { width: parent.width; text: qsTr("Cancel"); onClicked: deleteConfirmation.close() }
        }
    }
    header: Item {
        id: titleBar
        height: 84 + window.safeTop
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: window.safeTop
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 12
            Image { source: "qrc:/icons/localclipboard-64.png"; sourceSize.width: 40; sourceSize.height: 40; Layout.preferredWidth: 40; Layout.preferredHeight: 40 }
            Column {
                Layout.fillWidth: true
                spacing: 3
                BodyText { width: parent.width; elide: Text.ElideRight; wrapMode: Text.NoWrap; text: "LocalClipboard"; font.pixelSize: 20; font.weight: Font.DemiBold }
                BodyText { width: parent.width; elide: Text.ElideRight; wrapMode: Text.NoWrap; text: qsTr("Your space. Your devices."); font.pixelSize: 12; color: Theme.muted }
            }
            ActionButton {
                visible: window.page !== "home" && controller.registered
                text: qsTr("Back")
                subtle: true
                onClicked: window.page = "home"
                Layout.maximumWidth: 110
            }
        }
    }
    footer: Item {
        id: tabBar
        visible: !window.wide && controller.registered && window.page !== "register" && window.page !== "pair"
        height: visible ? 84 + window.safeBottom : 0
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }
        Navigation {
            anchors.fill: parent
            anchors.margins: 12
            anchors.bottomMargin: 12 + window.safeBottom
            currentPage: window.page
            onSelected: function(page) { window.navigate(page) }
        }
    }
    Rectangle {
        visible: window.wide && controller.registered
        width: 220
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: Theme.background
        Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: Theme.line }
        Column {
            anchors.fill: parent; anchors.margins: 16; spacing: 12
            Navigation {
                width: parent.width
                vertical: true
                currentPage: window.page
                onSelected: function(page) { window.navigate(page) }
            }
            BodyText { width: parent.width; text: controller.deviceName; font.pixelSize: 13; color: Theme.muted }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: window.wide && controller.registered ? 220 : 0
        spacing: 0
        // Persistent feedback does not resize or reset a scrolling page on each poll.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: notice.implicitHeight + 24
            visible: controller.message.length > 0
            color: Theme.elevated
            BodyText { id: notice; x: 24; y: 12; width: parent.width - 48; text: controller.message; color: Theme.accent; font.pixelSize: 14 }
        }
        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: ["home", "settings", "register", "pair", "files", "devices", "transfers", "history", "diagnostics"].indexOf(window.page)
            ExchangePage {
                controller: window.controller
                platformName: window.platformName
                pendingApprovals: window.pendingApprovals
                onNavigate: function(destination) { window.navigate(destination) }
                onPairRequested: window.showPairing()
            }
            ScrollPage {
                objectName: "settingsScroll"
                BodyText { width: parent.width; text: qsTr("Settings"); font.pixelSize: 30; font.weight: Font.DemiBold }
                Card {
                    width: parent.width
                    BodyText { width: parent.width; text: qsTr("Your profile"); font.pixelSize: 19; font.weight: Font.DemiBold }
                    BodyText { width: parent.width; text: controller.ownerName + "\n" + controller.deviceName; color: Theme.muted }
                    ActionButton { width: parent.width; text: qsTr("Connect device…"); onClicked: window.showPairing() }
                    ActionButton { width: parent.width; text: qsTr("Change profile…"); subtle: true; onClicked: { window.changingProfile = true; window.page = "register" } }
                }
                Card {
                    width: parent.width
                    SettingSwitch {
                        objectName: "pauseSwitch"
                        width: parent.width
                        text: qsTr("Pause synchronization")
                        checked: controller.paused
                        onToggled: controller.setPaused(checked)
                    }
                    ActionButton { width: parent.width; text: qsTr("Pause for 15 minutes"); onClicked: controller.pauseFor15Minutes() }
                    SettingSwitch {
                        objectName: "automaticClipboardSwitch"
                        width: parent.width
                        visible: controller.automaticClipboardSupported
                        text: qsTr("Automatically read and send clipboard")
                        checked: controller.automaticClipboard
                        onToggled: controller.setAutomaticClipboard(checked)
                    }
                    BodyText { width: parent.width; text: qsTr("Clipboard reading is off by default. Received items stay on Home until you choose Copy."); color: Theme.muted; font.pixelSize: 14 }
                    ActionButton { width: parent.width; text: qsTr("Send from clipboard"); enabled: !controller.busy && !controller.paused && controller.sendingEnabled; onClicked: controller.sendClipboard() }
                    SettingSwitch { width: parent.width; text: qsTr("Allow sending"); checked: controller.sendingEnabled; onToggled: controller.setSendingEnabled(checked) }
                    SettingSwitch { width: parent.width; text: qsTr("Allow receiving"); checked: controller.receivingEnabled; onToggled: controller.setReceivingEnabled(checked) }
                    SettingSwitch {
                        objectName: "autostartSwitch"
                        width: parent.width
                        visible: window.platformName !== "ios"
                        text: qsTr("Start automatically")
                        checked: controller.autostart
                        onToggled: controller.setAutostart(checked)
                    }
                }
                Card {
                    width: parent.width
                    ActionButton { width: parent.width; visible: window.platformName === "android"; text: qsTr("Notification settings"); onClicked: controller.notificationSettings() }
                    BodyText { width: parent.width; visible: window.platformName === "android"; text: qsTr("Enable pop-up notifications for Received clipboard in Android settings. Silent mode and Do Not Disturb may hide banners."); color: Theme.muted; font.pixelSize: 14 }
                    BodyText { width: parent.width; text: qsTr("Language"); font.weight: Font.DemiBold }
                    RowLayout {
                        width: parent.width
                        ActionButton { Layout.fillWidth: true; text: "Русский"; primary: controller.language === "ru"; onClicked: controller.language = "ru" }
                        ActionButton { Layout.fillWidth: true; text: "English"; primary: controller.language === "en"; onClicked: controller.language = "en" }
                    }
                }
                Card {
                    width: parent.width
                    BodyText { width: parent.width; text: qsTr("Network settings"); font.weight: Font.DemiBold }
                    BodyText { width: parent.width; text: qsTr("If a device is not found automatically, enter its local IPv4 address. Separate addresses with spaces."); color: Theme.muted; font.pixelSize: 14 }
                    BodyText { width: parent.width; text: qsTr("This device: %1").arg(controller.localAddresses || "—"); color: Theme.muted; font.pixelSize: 14 }
                    Editor { id: addresses; width: parent.width; text: controller.addresses; placeholderText: "192.168.1.10"; maximumLength: 512; inputMethodHints: Qt.ImhPreferNumbers; onAccepted: saveNetwork.clicked() }
                    ActionButton { id: saveNetwork; width: parent.width; text: qsTr("Save"); onClicked: { if (controller.saveAddresses(addresses.text)) Qt.inputMethod.hide() } }
                }
                Card {
                    width: parent.width
                    visible: controller.sourceExclusionsSupported
                    BodyText { width: parent.width; text: qsTr("Exclude source applications"); font.weight: Font.DemiBold }
                    BodyText { width: parent.width; text: qsTr("Automatic sending is skipped while a listed app is active. Enter a macOS bundle identifier or a Windows executable name. Manual sending remains available."); color: Theme.muted; font.pixelSize: 14 }
                    TextArea { id: exclusions; width: parent.width; text: controller.excludedApplications; placeholderText: "com.apple.keychainaccess"; color: Theme.text; wrapMode: TextEdit.Wrap; selectByMouse: !window.phone; background: Rectangle { color: Theme.background; radius: 12 } }
                    ActionButton { width: parent.width; text: qsTr("Save"); onClicked: controller.setExcludedApplications(exclusions.text) }
                }
                ActionButton { width: parent.width; text: qsTr("Network and diagnostics"); onClicked: window.navigate("diagnostics") }
                ActionButton { width: parent.width; text: qsTr("Devices and access"); onClicked: window.navigate("devices") }
                ActionButton { width: parent.width; text: qsTr("Local history"); onClicked: window.navigate("history") }
                Card {
                    width: parent.width
                    visible: !window.phone
                    ActionButton { width: parent.width; text: qsTr("Uninstall…"); subtle: true; onClicked: controller.uninstall() }
                    ActionButton { width: parent.width; text: qsTr("Quit LocalClipboard"); onClicked: controller.quit() }
                }
                BodyText { width: parent.width; text: controller.managedAccess ? qsTr("New devices need the profile owner’s approval. Each person should create their own profile.") : qsTr("Your connection code gives access to this profile. Each person should create their own profile."); color: Theme.muted; font.pixelSize: 14 }
            }
            ScrollPage {
                objectName: "registrationScroll"
                BodyText { width: parent.width; text: qsTr("Make it yours."); font.pixelSize: 32; font.weight: Font.DemiBold }
                BodyText { width: parent.width; text: qsTr("Create a private space for your devices, or join one you already use."); color: Theme.muted }
                RowLayout {
                    width: parent.width
                    ActionButton { Layout.fillWidth: true; text: "Русский"; primary: controller.language === "ru"; onClicked: controller.language = "ru" }
                    ActionButton { Layout.fillWidth: true; text: "English"; primary: controller.language === "en"; onClicked: controller.language = "en" }
                }
                Card {
                    width: parent.width
                    BodyText { width: parent.width; text: qsTr("Your name"); color: Theme.muted; font.pixelSize: 14 }
                    Editor { id: owner; objectName: "ownerInput"; width: parent.width; text: controller.ownerName; placeholderText: qsTr("Your name"); maximumLength: 80; onAccepted: device.forceActiveFocus() }
                    BodyText { width: parent.width; text: qsTr("Device name"); color: Theme.muted; font.pixelSize: 14 }
                    Editor { id: device; objectName: "deviceInput"; width: parent.width; text: controller.deviceName; placeholderText: qsTr("Device name"); maximumLength: 80; onAccepted: Qt.inputMethod.hide() }
                    RowLayout {
                        id: modes
                        objectName: "profileMode"
                        property bool join: false
                        width: parent.width
                        ActionButton { Layout.fillWidth: true; text: qsTr("Create new"); primary: !modes.join; onClicked: modes.join = false }
                        ActionButton { Layout.fillWidth: true; text: qsTr("Connect"); primary: modes.join; onClicked: modes.join = true }
                    }
                    Column {
                        width: parent.width; spacing: 14; visible: modes.join
                        BodyText { width: parent.width; text: qsTr("Connection code"); color: Theme.muted; font.pixelSize: 14 }
                        Editor { id: code; objectName: "codeInput"; width: parent.width; placeholderText: "LC3.… / LC2.…"; maximumLength: 1024; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhPreferLatin }
                        BodyText { width: parent.width; text: qsTr("Addresses from QR: %1").arg(controller.parseConnection(code.text).addresses || "—"); color: Theme.muted; font.pixelSize: 14 }
                        ActionButton { width: parent.width; text: qsTr("Scan QR code"); onClicked: { Qt.inputMethod.hide(); const value = controller.scanQr(); if (value.length) code.text = value } }
                    }
                    ActionButton {
                        objectName: "registerAction"
                        width: parent.width; primary: true
                        text: modes.join ? qsTr("Connect this device") : qsTr("Create my profile")
                        onClicked: {
                            if (!controller.registerDevice(owner.text, device.text, modes.join, code.text)) return
                            Qt.inputMethod.hide(); code.clear()
                            if (modes.join) window.page = "home"; else window.showPairing()
                        }
                    }
                }
                BodyText { width: parent.width; text: qsTr("Only devices with your code can synchronize. Use a separate profile for another person."); color: Theme.muted; font.pixelSize: 14 }
            }
            ScrollPage {
                objectName: "pairingScroll"
                BodyText { width: parent.width; text: qsTr("Connect another device"); font.pixelSize: 30; font.weight: Font.DemiBold }
                BodyText { width: parent.width; text: qsTr("On your other device, choose Connect and scan this code. The profile and IP addresses are filled in together."); color: Theme.muted }
                Card {
                    width: parent.width
                    Rectangle {
                        width: Math.min(300, parent.width); height: width; radius: 20; color: "white"; anchors.horizontalCenter: parent.horizontalCenter
                        Image {
                            anchors.fill: parent; anchors.margins: 12; cache: false; smooth: false
                            source: window.page === "pair" ? "image://invitation/current?revision=" + window.qrRevision : ""
                        }
                    }
                    BodyText { width: parent.width; text: qsTr("Keep this QR code private. Anyone with it can join your profile."); color: Theme.accent; font.pixelSize: 14 }
                    BodyText { width: parent.width; visible: controller.managedAccess; text: qsTr("The profile owner must approve the new device under Devices and access after scanning."); color: Theme.muted; font.pixelSize: 14 }
                    ActionButton { width: parent.width; text: qsTr("Copy connection code"); onClicked: controller.copyInvitation() }
                    ActionButton { width: parent.width; text: qsTr("Save QR image…"); subtle: true; onClicked: controller.saveQr() }
                }
                ActionButton { width: parent.width; primary: true; text: qsTr("Done"); onClicked: window.page = "home" }
            }
            ColumnLayout {
                spacing: 16
                BodyText { Layout.fillWidth: true; Layout.leftMargin: 24; Layout.rightMargin: 24; text: qsTr("Received files"); font.pixelSize: 30; font.weight: Font.DemiBold }
                BodyText { Layout.fillWidth: true; Layout.leftMargin: 24; Layout.rightMargin: 24; text: controller.loadingFiles ? qsTr("Loading…") : qsTr("Tap a file to open it. Local copies stay on this device."); color: Theme.muted; font.pixelSize: 14 }
                ListView {
                    objectName: "receivedList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    cacheBuffer: 250
                    model: controller.receivedFiles
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { width: window.phone ? 3 : 8; interactive: !window.phone; Accessible.ignored: true }
                    enabled: !controller.loadingFiles
                    delegate: Card {
                        required property var modelData
                        required property int index
                        width: Math.min(ListView.view.width - 40, 720)
                        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                        Image { width: 120; height: 90; visible: modelData.thumbnail.length > 0; source: modelData.thumbnail; sourceSize.width: 240; sourceSize.height: 180; fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false }
                        BodyText { width: parent.width; text: modelData.name; font.weight: Font.DemiBold }
                        BodyText { width: parent.width; text: modelData.size; color: Theme.muted; font.pixelSize: 13 }
                        ActionButton { width: parent.width; text: qsTr("Send again"); enabled: !controller.busy && !controller.paused && controller.sendingEnabled; onClicked: controller.resendFile(index) }
                        RowLayout {
                            width: parent.width
                            spacing: 10
                            ActionButton { Layout.fillWidth: true; text: qsTr("Open"); font.pixelSize: 14; onClicked: controller.openReceivedFile(index) }
                            ActionButton { Layout.fillWidth: true; text: qsTr("Delete"); font.pixelSize: 14; subtle: true; onClicked: { deleteConfirmation.fileIndex = index; deleteConfirmation.open() } }
                        }
                    }
                    BodyText { anchors.centerIn: parent; width: parent.width - 48; horizontalAlignment: Text.AlignHCenter; visible: !controller.loadingFiles && controller.receivedFiles.length === 0; text: qsTr("Files and photos you receive will appear here."); color: Theme.muted }
                }
            }
            DevicesPage { controller: window.controller; onUpgradeRequested: upgradeDialog.open() }
            TransfersPage { controller: window.controller }
            HistoryPage { controller: window.controller }
            DiagnosticsPage { controller: window.controller }
        }
    }
    Dialog {
        id: upgradeDialog
        anchors.centerIn: parent; width: Math.min(window.width - 32, 440); modal: true; padding: 24
        background: Rectangle { radius: 24; color: Theme.surface; border.color: Theme.line }
        contentItem: Column {
            spacing: 16
            BodyText { width: parent.width; text: qsTr("Upgrade device access?"); font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: qsTr("A new connection code will replace the shared key. Reconnect and approve every other device. This device becomes the profile owner. Local history is cleared."); color: Theme.muted }
            ActionButton { width: parent.width; primary: true; text: qsTr("Upgrade and reconnect"); onClicked: { if (controller.upgradeAccess()) { upgradeDialog.close(); window.showPairing() } } }
            ActionButton { width: parent.width; text: qsTr("Cancel"); onClicked: upgradeDialog.close() }
        }
    }
}

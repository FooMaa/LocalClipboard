import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme.js" as Theme

ApplicationWindow {
    id: window
    required property QtObject controller
    width: 412
    height: 860
    visible: true
    title: "LocalClipboard"
    color: Theme.background
    font.pixelSize: 16
    property string page: controller.registered ? "home" : "register"
    property bool changingProfile: false
    property int qrRevision: 0
    // ApplicationWindow creates this internal Control in C++, outside QML's
    // componentComplete path. Initialize its accessibility attachment here too.
    Component.onCompleted: contentItem.parent.Accessible.name = title
    function showPairing() { qrRevision++; page = "pair" }
    function back() {
        if (page === "register" && !controller.registered) return false
        if (page !== "home") { page = "home"; return true }
        return false
    }
    onClosing: function(close) { if (back()) close.accepted = false }
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
        height: 84 + SafeArea.margins.top
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: titleBar.SafeArea.margins.top
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
        visible: controller.registered && window.page !== "register" && window.page !== "pair"
        height: visible ? 84 + SafeArea.margins.bottom : 0
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            anchors.bottomMargin: 12 + tabBar.SafeArea.margins.bottom
            spacing: 8
            Repeater {
                model: [qsTr("Clipboard"), qsTr("Files"), qsTr("Settings")]
                ActionButton {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData
                    primary: window.page === ["home", "files", "settings"][index]
                    subtle: !primary
                    onClicked: {
                        window.page = ["home", "files", "settings"][index]
                        if (index === 1) controller.refreshFiles()
                    }
                }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
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
            currentIndex: ["home", "settings", "register", "pair", "files"].indexOf(window.page)
            ScrollPage {
                objectName: "homeScroll"
                BodyText { width: parent.width; text: qsTr("One clipboard,\nall your devices."); font.pixelSize: 32; font.weight: Font.DemiBold; lineHeight: 1.12 }
                Card {
                    width: parent.width
                    Row {
                        width: parent.width; spacing: 10
                        Rectangle { width: 9; height: 9; radius: 5; color: controller.paused ? Theme.muted : Theme.positive; anchors.verticalCenter: parent.verticalCenter }
                        BodyText { width: parent.width - 19; text: controller.paused ? qsTr("Synchronization paused") : qsTr("Ready to receive"); color: controller.paused ? Theme.muted : Theme.positive; font.weight: Font.DemiBold }
                    }
                    BodyText { width: parent.width; text: controller.ownerName + " · " + controller.deviceName; color: Theme.muted; font.pixelSize: 14 }
                    ActionButton { width: parent.width; text: controller.busy ? qsTr("Preparing files…") : qsTr("Send clipboard"); primary: true; enabled: !controller.busy && !controller.paused; onClicked: controller.sendClipboard() }
                    ActionButton { width: parent.width; text: qsTr("Send files / photos…"); enabled: !controller.busy && !controller.paused; onClicked: controller.pickFiles() }
                }
                Row {
                    width: parent.width
                    BodyText { width: parent.width - 44; text: qsTr("Your devices"); font.pixelSize: 21; font.weight: Font.DemiBold }
                    BodyText { width: 44; horizontalAlignment: Text.AlignRight; text: controller.peers.length; color: Theme.muted }
                }
                Card {
                    width: parent.width
                    visible: controller.peers.length === 0
                    BodyText { width: parent.width; text: qsTr("Bring your devices together"); font.weight: Font.DemiBold }
                    BodyText { width: parent.width; text: qsTr("Open LocalClipboard on another device, connect to this profile and use the same Wi-Fi."); color: Theme.muted; font.pixelSize: 14 }
                    ActionButton { width: parent.width; text: qsTr("Connect device…"); onClicked: window.showPairing() }
                }
                Repeater {
                    model: controller.peers
                    Card {
                        required property string modelData
                        width: parent.width
                        BodyText { width: parent.width; text: modelData; font.weight: Font.DemiBold }
                        BodyText { width: parent.width; text: qsTr("Connected on your network"); color: Theme.positive; font.pixelSize: 13 }
                    }
                }
                BodyText { width: parent.width; text: qsTr("Background reception stays on. To send, open the app or use Share → LocalClipboard."); color: Theme.muted; font.pixelSize: 14 }
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
                        width: parent.width
                        text: qsTr("Pause synchronization")
                        checked: controller.paused
                        onToggled: controller.setPaused(checked)
                    }
                    SettingSwitch {
                        width: parent.width
                        text: qsTr("Start automatically")
                        checked: controller.autostart
                        onToggled: controller.setAutostart(checked)
                    }
                }
                Card {
                    width: parent.width
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
                    Editor { id: addresses; width: parent.width; text: controller.addresses; placeholderText: "192.168.1.10"; maximumLength: 512; inputMethodHints: Qt.ImhPreferNumbers; onAccepted: saveNetwork.clicked() }
                    ActionButton { id: saveNetwork; width: parent.width; text: qsTr("Save"); onClicked: { if (controller.saveAddresses(addresses.text)) Qt.inputMethod.hide() } }
                }
                BodyText { width: parent.width; text: qsTr("Your connection code gives access to this profile. Each person should create their own profile."); color: Theme.muted; font.pixelSize: 14 }
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
                        property bool join: false
                        width: parent.width
                        ActionButton { Layout.fillWidth: true; text: qsTr("Create new"); primary: !modes.join; onClicked: modes.join = false }
                        ActionButton { Layout.fillWidth: true; text: qsTr("Connect"); primary: modes.join; onClicked: modes.join = true }
                    }
                    Column {
                        width: parent.width; spacing: 14; visible: modes.join
                        BodyText { width: parent.width; text: qsTr("Connection code"); color: Theme.muted; font.pixelSize: 14 }
                        Editor { id: code; objectName: "codeInput"; width: parent.width; placeholderText: "LC2.…"; maximumLength: 68; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhPreferLatin }
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
                BodyText { width: parent.width; text: qsTr("On your other device, choose Connect and scan this code."); color: Theme.muted }
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
                    ScrollBar.vertical: ScrollBar { width: 3; interactive: false; Accessible.ignored: true }
                    enabled: !controller.loadingFiles
                    delegate: Card {
                        required property var modelData
                        required property int index
                        width: ListView.view.width - 40
                        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                        BodyText { width: parent.width; text: modelData.name; font.weight: Font.DemiBold }
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
        }
    }
}

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "Theme.js" as Theme

ScrollPage {
    id: page
    objectName: "homeScroll"
    required property QtObject controller
    property string platformName: Qt.platform.os
    property int pendingApprovals: 0
    readonly property bool canSend: controller.registered && !controller.paused && !controller.busy
        && controller.sendingEnabled && controller.accessState === "approved" && controller.peers.length > 0
    signal navigate(string destination)
    signal pairRequested()

    BodyText { width: parent.width; text: qsTr("Share with your devices"); font.pixelSize: 28; font.weight: Font.DemiBold }
    RowLayout {
        width: parent.width
        Column {
            Layout.fillWidth: true
            spacing: 4
            BodyText {
                width: parent.width
                text: controller.paused ? qsTr("Exchange paused") : controller.peers.length > 0
                    ? qsTr("Devices online: %1").arg(controller.peers.length) : qsTr("No devices online")
                color: controller.peers.length > 0 && !controller.paused ? Theme.positive : Theme.muted
            }
            BodyText { width: parent.width; text: controller.deviceName; color: Theme.muted; font.pixelSize: 13 }
        }
        ActionButton { text: qsTr("Connect"); onClicked: page.pairRequested() }
    }
    Card {
        width: parent.width
        visible: pendingApprovals > 0 || (controller.managedAccess && controller.accessState !== "approved")
        BodyText { width: parent.width; text: page.pendingApprovals > 0 ? qsTr("Devices waiting for approval: %1").arg(page.pendingApprovals)
            : controller.accessState === "revoked" ? qsTr("This device's access was revoked. Register again and ask the owner to approve the new fingerprint.")
            : qsTr("Waiting for approval on the profile owner's device."); color: Theme.accent }
        ActionButton { width: parent.width; text: qsTr("Devices and access"); onClicked: page.navigate("devices") }
    }
    Card {
        width: parent.width
        BodyText { width: parent.width; text: qsTr("Send something"); font.pixelSize: 20; font.weight: Font.DemiBold }
        ScrollView {
            id: composer
            width: parent.width
            height: 100
            contentWidth: availableWidth
            clip: true
            TextArea {
                id: draft
                objectName: "exchangeDraft"
                width: composer.availableWidth
                implicitHeight: Math.max(100, contentHeight + topPadding + bottomPadding)
                padding: 14
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                clip: true
                color: Theme.text
                placeholderTextColor: Theme.muted
                selectionColor: Theme.accent
                selectedTextColor: Theme.background
                placeholderText: qsTr("Write or paste text here…")
                selectByMouse: Qt.platform.os !== "android" && Qt.platform.os !== "ios"
                background: Rectangle { radius: 14; color: Theme.background; border.color: draft.activeFocus ? Theme.accent : Theme.line }
                Accessible.name: placeholderText
            }
        }
        RowLayout {
            width: parent.width
            ActionButton { Layout.fillWidth: true; text: qsTr("Paste"); onClicked: { draft.forceActiveFocus(); draft.paste() } }
            ActionButton { objectName: "sendTextButton"; Layout.fillWidth: true; primary: true; text: qsTr("Send text"); enabled: page.canSend && draft.text.trim().length > 0; onClicked: { controller.sendText(draft.text); Qt.inputMethod.hide() } }
        }
        ActionButton { width: parent.width; text: qsTr("Choose photos or files…"); enabled: page.canSend; onClicked: controller.pickFiles() }
        BodyText { width: parent.width; visible: controller.peers.length === 0; text: qsTr("Connect another device on the same Wi-Fi to send."); color: Theme.muted; font.pixelSize: 13 }
        BodyText { width: parent.width; visible: controller.automaticClipboard; text: qsTr("Automatic clipboard sending is on."); color: Theme.muted; font.pixelSize: 13 }
    }
    RowLayout {
        width: parent.width
        BodyText { Layout.fillWidth: true; text: qsTr("Recent exchange"); font.pixelSize: 21; font.weight: Font.DemiBold }
        ActionButton { visible: controller.exchangeItems.length > 0; text: qsTr("Clear"); subtle: true; onClicked: clearDialog.open() }
    }
    Card {
        width: parent.width
        visible: controller.exchangeItems.length === 0
        BodyText { width: parent.width; text: qsTr("Text, photos and files arrive here."); font.weight: Font.DemiBold }
        BodyText { width: parent.width; text: qsTr("Choose Copy on an item, then paste it into another app. Receiving never replaces your clipboard automatically."); color: Theme.muted; font.pixelSize: 14 }
    }
    Repeater {
        model: controller.exchangeItems
        Card {
            required property var modelData
            width: parent.width
            RowLayout {
                width: parent.width
                BodyText { Layout.fillWidth: true; text: modelData.outgoing ? qsTr("From this device") : qsTr("Received"); color: modelData.outgoing ? Theme.muted : Theme.positive; font.pixelSize: 13 }
                BodyText { text: Qt.formatDateTime(new Date(modelData.time), "dd MMM, hh:mm"); color: Theme.muted; font.pixelSize: 12 }
            }
            Image { width: parent.width; height: visible ? 160 : 0; visible: modelData.thumbnail.length > 0; source: modelData.thumbnail; fillMode: Image.PreserveAspectFit; asynchronous: true; sourceSize.width: 320; sourceSize.height: 200 }
            BodyText { width: parent.width; visible: modelData.text.length > 0; text: modelData.text; maximumLineCount: 5; elide: Text.ElideRight }
            BodyText { width: parent.width; visible: modelData.kind !== "text"; text: (modelData.kind === "image" ? qsTr("Photo") : qsTr("Files")) + " · " + (modelData.bytes / 1024).toFixed(1) + " KB"; color: Theme.muted; font.pixelSize: 13 }
            ActionButton { objectName: "copyExchangeButton"; width: parent.width; text: qsTr("Copy"); primary: !modelData.outgoing; enabled: !controller.busy; onClicked: controller.copyExchangeItem(modelData.id) }
        }
    }
    BodyText { width: parent.width; visible: controller.exchangeItems.length > 0; text: qsTr("Up to 20 items, 64 MiB, for 7 days on this device. Clear removes only this list."); color: Theme.muted; font.pixelSize: 12 }
    RowLayout {
        width: parent.width
        ActionButton { Layout.fillWidth: true; text: qsTr("Devices"); onClicked: page.navigate("devices") }
        ActionButton { Layout.fillWidth: true; text: qsTr("Transfers"); onClicked: page.navigate("transfers") }
    }
    BodyText { width: parent.width; visible: platformName === "ios"; text: qsTr("Keep LocalClipboard open to receive on iPhone or iPad."); color: Theme.muted; font.pixelSize: 13 }
    Dialog {
        id: clearDialog
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        modal: true
        padding: 24
        background: Rectangle { color: Theme.surface; radius: 24; border.color: Theme.line }
        contentItem: Column {
            spacing: 16
            BodyText { width: parent.width; text: qsTr("Clear recent exchange on this device?"); font.weight: Font.DemiBold }
            ActionButton { width: parent.width; text: qsTr("Clear"); primary: true; onClicked: { controller.clearExchange(); clearDialog.close() } }
            ActionButton { width: parent.width; text: qsTr("Cancel"); onClicked: clearDialog.close() }
        }
    }
}

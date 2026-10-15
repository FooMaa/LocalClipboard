import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as Theme
ScrollPage {
    id: page
    required property QtObject controller
    signal upgradeRequested()
    BodyText { width: parent.width; text: qsTr("Devices and access"); font.pixelSize: 28; font.weight: Font.DemiBold }
    Card {
        width: parent.width
        BodyText { width: parent.width; text: page.controller.managedAccess ? qsTr("Separate keys for every device") : qsTr("Legacy shared-code profile"); font.weight: Font.DemiBold }
        BodyText { width: parent.width; text: page.controller.managedAccess ? qsTr("Compare the fingerprint on both screens before approving. Only the profile owner's device can approve or revoke access. Revocation reaches other devices when they reconnect; keep the owner's device available.") : qsTr("This profile uses a shared key. Upgrade to revoke individual devices. All devices will need to reconnect with the new QR code."); color: Theme.muted; font.pixelSize: 14 }
        BodyText { width: parent.width; visible: page.controller.managedAccess; text: qsTr("This device's fingerprint: %1").arg(page.controller.fingerprint); color: Theme.accent; font.pixelSize: 14 }
        ActionButton { width: parent.width; visible: !page.controller.managedAccess; text: qsTr("Upgrade device access…"); onClicked: page.upgradeRequested() }
        Editor { id: deviceName; width: parent.width; text: page.controller.deviceName; placeholderText: qsTr("Device name"); maximumLength: 80 }
        ActionButton { width: parent.width; text: qsTr("Rename this device"); onClicked: page.controller.renameDevice(deviceName.text) }
    }
    BodyText { width: parent.width; visible: page.controller.devices.length === 0; text: qsTr("No other devices have been seen yet."); color: Theme.muted }
    Repeater {
        model: page.controller.devices
        Card {
            required property var modelData
            width: parent.width
            BodyText { width: parent.width; text: modelData.name; font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: modelData.revoked ? qsTr("Access revoked") : modelData.approved ? (modelData.online ? qsTr("Online") : qsTr("Offline")) : qsTr("Waiting for approval"); color: modelData.revoked ? Theme.muted : Theme.accent }
            BodyText { width: parent.width; text: qsTr("Last seen: %1").arg(new Date(modelData.seen * 1000).toLocaleString(Qt.locale(), Locale.ShortFormat)); color: Theme.muted; font.pixelSize: 13 }
            BodyText { width: parent.width; visible: page.controller.managedAccess; text: qsTr("Fingerprint: %1").arg(modelData.fingerprint); color: Theme.muted; font.pixelSize: 13 }
            ActionButton {
                width: parent.width
                visible: page.controller.administrator && !modelData.revoked
                text: modelData.approved ? qsTr("Revoke access…") : qsTr("Approve device…")
                onClicked: { accessDialog.deviceId = modelData.id; accessDialog.revoke = modelData.approved; accessDialog.deviceName = modelData.name; accessDialog.open() }
            }
        }
    }
    Dialog {
        id: accessDialog
        parent: Overlay.overlay
        property string deviceId
        property string deviceName
        property bool revoke
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 440)
        modal: true
        padding: 24
        background: Rectangle { radius: 24; color: Theme.surface; border.color: Theme.line }
        contentItem: Column {
            spacing: 16
            BodyText { width: parent.width; text: accessDialog.revoke ? qsTr("Revoke access for %1?").arg(accessDialog.deviceName) : qsTr("Approve %1?").arg(accessDialog.deviceName); font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: accessDialog.revoke ? qsTr("This device will no longer be able to send or receive. Devices that are offline learn about the revocation when they reconnect.") : qsTr("Only approve a device you control after comparing its fingerprint."); color: Theme.muted }
            ActionButton { width: parent.width; primary: true; text: qsTr("Confirm"); onClicked: { page.controller.authorizeDevice(accessDialog.deviceId, accessDialog.revoke); accessDialog.close() } }
            ActionButton { width: parent.width; text: qsTr("Cancel"); onClicked: accessDialog.close() }
        }
    }
}

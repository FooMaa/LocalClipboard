import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as Theme
ScrollPage {
    id: page
    required property QtObject controller
    BodyText { width: parent.width; text: qsTr("Transfers"); font.pixelSize: 30; font.weight: Font.DemiBold }
    BodyText { width: parent.width; text: qsTr("Sending → received on the device → copied to its clipboard. On Android, the last step waits for Copy. Cancellation cannot recall content already received."); color: Theme.muted; font.pixelSize: 14 }
    BodyText { width: parent.width; visible: page.controller.transfers.length === 0; text: qsTr("Your next transfer will appear here."); color: Theme.muted }
    Repeater {
        model: page.controller.transfers
        Card {
            required property var modelData
            width: parent.width
            BodyText { width: parent.width; text: modelData.device; font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: modelData.kind === "files" ? qsTr("%1 file(s)").arg(modelData.items) : modelData.kind === "image" ? qsTr("image") : qsTr("text (%1 characters)").arg(modelData.items); color: Theme.muted; font.pixelSize: 14 }
            BodyText { width: parent.width; text: ({sending: qsTr("Sending"), received: qsTr("Received on device"), copied: qsTr("Copied to clipboard"), failed: qsTr("Failed"), cancelled: qsTr("Cancelled")})[modelData.state] || modelData.state; color: Theme.accent }
            ProgressBar { width: parent.width; from: 0; to: 100; value: modelData.progress; visible: modelData.state === "sending"; Accessible.name: qsTr("Transfer progress") }
            BodyText { width: parent.width; visible: modelData.error.length > 0; text: qsTr("Details: %1").arg(page.controller.translateStatus(modelData.error)); color: Theme.muted; font.pixelSize: 14 }
            ActionButton { width: parent.width; visible: modelData.state === "sending"; text: qsTr("Cancel"); onClicked: page.controller.cancelTransfer(modelData.id) }
            ActionButton { width: parent.width; visible: modelData.retryable && (modelData.state === "failed" || modelData.state === "cancelled"); text: qsTr("Retry"); onClicked: page.controller.retryTransfer(modelData.id) }
        }
    }
}

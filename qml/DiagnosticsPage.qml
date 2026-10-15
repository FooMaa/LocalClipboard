import QtQuick
import "Theme.js" as Theme
ScrollPage {
    id: page
    required property QtObject controller
    BodyText { width: parent.width; text: qsTr("Network and diagnostics"); font.pixelSize: 28; font.weight: Font.DemiBold }
    BodyText { width: parent.width; text: qsTr("Use the same Wi-Fi, allow LocalClipboard through the firewall, and keep the iOS app open. Guest networks and VPNs may isolate devices."); color: Theme.muted }
    BodyText { width: parent.width; text: qsTr("On Mac, allow LocalClipboard under System Settings → Privacy & Security → Local Network. After scanning, approve the new device under Devices and access."); color: Theme.muted }
    Card {
        width: parent.width
        ActionButton { width: parent.width; text: qsTr("Check network"); primary: true; onClicked: page.controller.checkNetwork() }
        BodyText { width: parent.width; text: page.controller.diagnostics; font.pixelSize: 13; color: Theme.muted }
        ActionButton { width: parent.width; text: qsTr("Export diagnostic report…"); onClicked: page.controller.exportDiagnostics() }
        ActionButton { width: parent.width; text: qsTr("Copy diagnostic report"); onClicked: page.controller.copyDiagnostics() }
    }
    BodyText { width: parent.width; text: qsTr("Reports contain versions, network availability and setting flags. They exclude clipboard content, filenames, names, addresses, connection codes and keys."); color: Theme.muted; font.pixelSize: 14 }
}

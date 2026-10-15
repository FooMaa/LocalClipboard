import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "Theme.js" as Theme
ColumnLayout {
    id: page
    required property QtObject controller
    spacing: 12
    BodyText { Layout.fillWidth: true; Layout.leftMargin: 24; Layout.rightMargin: 24; text: qsTr("Local history"); font.pixelSize: 30; font.weight: Font.DemiBold }
    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 12
        cacheBuffer: 200
        reuseItems: true
        model: page.controller.history
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { Accessible.ignored: true }
        header: Column {
            width: Math.min(list.width - 40, 720)
            x: (list.width - width) / 2
            spacing: 14
            BodyText { width: parent.width; text: qsTr("History is optional, encrypted on this device and never synchronized as a database. Turning it off deletes saved items."); color: Theme.muted; font.pixelSize: 14 }
            Card {
                width: parent.width
                SettingSwitch { width: parent.width; text: qsTr("Keep local history"); checked: page.controller.historyEnabled; onToggled: page.controller.setHistoryEnabled(checked) }
                BodyText { width: parent.width; visible: page.controller.historyEnabled; text: qsTr("Keep for %1 days, up to %2 MiB and 100 items.").arg(page.controller.historyDays).arg(page.controller.historyMiB); color: Theme.muted; font.pixelSize: 14 }
                Row {
                    width: parent.width; spacing: 8; visible: page.controller.historyEnabled
                    ActionButton { width: (parent.width - 8) / 2; text: qsTr("1 day / 4 MiB"); onClicked: page.controller.setHistoryLimits(1, 4) }
                    ActionButton { width: (parent.width - 8) / 2; text: qsTr("7 days / 16 MiB"); onClicked: page.controller.setHistoryLimits(7, 16) }
                }
                Editor { id: query; width: parent.width; visible: page.controller.historyEnabled; placeholderText: qsTr("Search history"); maximumLength: 256; onTextChanged: searchTimer.restart() }
                Timer { id: searchTimer; interval: 200; onTriggered: page.controller.refreshHistory(query.text) }
                ActionButton { width: parent.width; visible: page.controller.historyEnabled; text: qsTr("Delete history…"); onClicked: clearDialog.open() }
            }
            Item { width: 1; height: 8 }
        }
        delegate: Card {
            required property var modelData
            width: Math.min(list.width - 40, 720)
            x: (list.width - width) / 2
            BodyText { width: parent.width; text: modelData.kind === "image" ? qsTr("image") : modelData.text; maximumLineCount: 5; elide: Text.ElideRight }
            BodyText { width: parent.width; text: new Date(modelData.time).toLocaleString(Qt.locale(), Locale.ShortFormat); color: Theme.muted; font.pixelSize: 13 }
            ActionButton { width: parent.width; text: qsTr("Copy again"); enabled: !page.controller.busy; onClicked: page.controller.copyHistory(modelData.id) }
        }
    }
    Dialog {
        id: clearDialog
        objectName: "clearHistoryDialog"
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent; width: Math.min(page.width - 32, 440); modal: true; padding: 24
        background: Rectangle { radius: 24; color: Theme.surface; border.color: Theme.line }
        contentItem: Column {
            spacing: 16
            BodyText { width: parent.width; text: qsTr("Delete all local history?"); font.weight: Font.DemiBold }
            ActionButton { width: parent.width; text: qsTr("Delete"); primary: true; onClicked: { page.controller.clearHistory(); clearDialog.close() } }
            ActionButton { width: parent.width; text: qsTr("Cancel"); onClicked: clearDialog.close() }
        }
    }
}

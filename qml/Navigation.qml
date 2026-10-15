import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Control {
    id: navigation
    property bool vertical: false
    property string currentPage: "home"
    signal selected(string page)

    implicitHeight: contentItem.implicitHeight
    contentItem: GridLayout {
        columns: navigation.vertical ? 1 : 3
        columnSpacing: 8
        rowSpacing: 12
        Repeater {
            model: [
                { page: "home", label: qsTr("Clipboard") },
                { page: "files", label: qsTr("Files") },
                { page: "settings", label: qsTr("Settings") }
            ]
            ActionButton {
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: !navigation.vertical
                text: modelData.label
                primary: navigation.currentPage === modelData.page
                subtle: !primary
                onClicked: navigation.selected(modelData.page)
            }
        }
    }
}

import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as Theme
TextField {
    id: control
    implicitHeight: 56
    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: Theme.background
    font.pixelSize: 16
    padding: 16
    selectByMouse: Qt.platform.os !== "android"
    inputMethodHints: Qt.ImhNoPredictiveText
    background: Rectangle {
        color: Theme.background
        radius: 16
        border.color: control.activeFocus ? Theme.accent : Theme.line
        border.width: control.activeFocus ? 2 : 1
    }
    Accessible.name: placeholderText
}

import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as Theme
Switch {
    id: control
    Accessible.name: text
    implicitHeight: Math.max(56, contentItem.implicitHeight + 16)
    spacing: 16
    padding: 0
    indicator: Rectangle {
        x: control.width - width
        y: (control.height - height) / 2
        width: 50; height: 30; radius: 15
        color: control.checked ? Theme.accent : Theme.line
        Rectangle {
            width: 22; height: 22; radius: 11; y: 4
            x: control.checked ? 24 : 4
            color: control.checked ? Theme.background : Theme.muted
            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: BodyText {
        Accessible.ignored: true
        text: control.text
        rightPadding: 66
        verticalAlignment: Text.AlignVCenter
    }
}

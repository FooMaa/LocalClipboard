import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as Theme
Button {
    id: control
    property bool primary: false
    property bool subtle: false
    Accessible.name: text
    implicitHeight: Math.max(54, label.implicitHeight + 28)
    implicitWidth: label.implicitWidth + 40
    leftPadding: 20
    rightPadding: 20
    topPadding: 14
    bottomPadding: 14
    opacity: enabled ? 1 : 0.45
    font.pixelSize: 16
    font.weight: Font.DemiBold
    contentItem: Text {
        id: label
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: control.primary ? Theme.background : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
    }
    background: Rectangle {
        radius: 18
        color: control.down ? Theme.line : control.primary ? Theme.accent : control.subtle ? "transparent" : Theme.elevated
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}

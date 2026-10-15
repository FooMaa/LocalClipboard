import QtQuick
import "Theme.js" as Theme
Text {
    color: Theme.text
    font.pixelSize: 16
    wrapMode: Text.Wrap
    textFormat: Text.PlainText
    Accessible.role: Accessible.StaticText
    Accessible.name: text
}

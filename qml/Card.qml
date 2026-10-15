import QtQuick
import "Theme.js" as Theme
Rectangle {
    id: card
    default property alias contents: body.data
    property int padding: 20
    implicitHeight: body.implicitHeight + padding * 2
    radius: 24
    color: Theme.surface
    Column {
        id: body
        x: card.padding
        y: card.padding
        width: parent.width - card.padding * 2
        spacing: 14
    }
}

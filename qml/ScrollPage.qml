import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as Theme
Flickable {
    id: flick
    default property alias contents: body.data
    contentWidth: width
    contentHeight: body.height + 40
    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    maximumFlickVelocity: 8000
    flickDeceleration: 2400
    clip: true
    Column {
        id: body
        width: Math.min(flick.width - 40, 720)
        x: (flick.width - width) / 2
        y: 12
        spacing: 20
    }
    ScrollBar.vertical: ScrollBar { width: Qt.platform.os === "android" || Qt.platform.os === "ios" ? 3 : 8; interactive: Qt.platform.os !== "android" && Qt.platform.os !== "ios"; Accessible.ignored: true }
    function revealEditor() {
        let editor = flick.Window.window ? flick.Window.window.activeFocusItem : null
        if (!editor || !editor.cursorRectangle || dragging || flicking) return
        let position = editor.mapToItem(contentItem, 0, editor.height)
        if (position.y > contentY + height - 24)
            contentY = Math.min(Math.max(0, contentHeight - height), position.y - height + 24)
        else if (position.y - editor.height < contentY)
            contentY = Math.max(0, position.y - editor.height - 24)
    }
    onHeightChanged: revealTimer.restart()
    Timer { id: revealTimer; interval: 120; onTriggered: flick.revealEditor() }
    Connections {
        target: flick.Window.window
        function onActiveFocusItemChanged() { revealTimer.restart() }
    }
}

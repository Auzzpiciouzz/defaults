import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?.
Popup {
    id: help
    modal: true
    anchors.centerIn: Overlay.overlay
    padding: 24
    Material.roundedScale: theme.radius

    background: Rectangle {
        color: "#17171b"
        border.color: "#2a2a30"
        radius: theme.radius
    }

    readonly property var shortcuts: [
        ["↑ ↓", qsTr("Move through the list")],
        ["Enter / Space", qsTr("Choose the app for the selected row")],
        ["Tab", qsTr("Switch between Categories and All file types")],
        ["/", qsTr("Search file types")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["F5", qsTr("Reload apps and defaults")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit")]
    ]

    contentItem: ColumnLayout {
        spacing: 14
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Label {
            text: qsTr("Keyboard shortcuts")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: "#f2f2f2"
        }

        GridLayout {
            columns: 2
            columnSpacing: 28
            rowSpacing: 10
            Repeater {
                model: help.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 0
                    text: modelData[0]
                    color: theme.accent
                    font.family: "monospace"
                }
            }
            Repeater {
                model: help.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 1
                    text: modelData[1]
                    color: "#d8d8dc"
                }
            }
        }
    }
}

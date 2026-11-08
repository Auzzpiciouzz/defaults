import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Lists the installed apps that can open a category or a type, and makes the
// chosen one the default.
Popup {
    id: picker

    property string kind: ""       // "category" or "type"
    property string target: ""     // a category id or a MIME type
    property string heading: ""
    property var apps: []

    signal chosen(string appId)

    function show(kindValue, targetValue, headingValue) {
        kind = kindValue
        target = targetValue
        heading = headingValue
        apps = kind === "category" ? backend.categoryCandidates(target) : backend.typeCandidates(target)
        var current = 0
        for (var i = 0; i < apps.length; ++i)
            if (apps[i].current) current = i
        list.currentIndex = current
        open()
        list.forceActiveFocus()
    }

    function choose(index) {
        if (index < 0 || index >= apps.length) return
        chosen(apps[index].id)
        close()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(520, parent ? parent.width - 48 : 520)
    height: Math.min(560, parent ? parent.height - 96 : 560)
    padding: 0
    Material.roundedScale: theme.radius

    background: Rectangle {
        color: "#17171b"
        border.color: "#2a2a30"
        radius: theme.radius
    }

    contentItem: ColumnLayout {
        spacing: 0
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Label {
            Layout.fillWidth: true
            Layout.margins: 20
            Layout.bottomMargin: 12
            text: qsTr("Open %1 with").arg(picker.heading)
            font.pixelSize: 18
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: "#f2f2f2"
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: picker.apps
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Keys.onReturnPressed: picker.choose(currentIndex)
            Keys.onEnterPressed: picker.choose(currentIndex)
            Keys.onSpacePressed: picker.choose(currentIndex)

            delegate: ItemDelegate {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 56
                leftPadding: 20
                rightPadding: 20
                highlighted: ListView.isCurrentItem
                onClicked: picker.choose(index)
                Material.roundedScale: Math.min(4, theme.radius)

                contentItem: RowLayout {
                    spacing: 14
                    AppIcon { name: row.modelData.icon; size: 32 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            elide: Text.ElideRight
                            color: "#f2f2f2"
                            font.pixelSize: 14
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: picker.kind === "category"
                            text: row.modelData.covered === row.modelData.total
                                  ? qsTr("Opens all of them")
                                  : qsTr("Opens %1 of %2 types").arg(row.modelData.covered).arg(row.modelData.total)
                            elide: Text.ElideRight
                            color: "#8a8a92"
                            font.pixelSize: 12
                        }
                    }
                    Label {
                        visible: row.modelData.current
                        text: qsTr("Current")
                        color: theme.accent
                        font.pixelSize: 12
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: picker.apps.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("No installed app says it can open these files. Install one, and it shows up here.")
                color: "#8a8a92"
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Enter to choose · Esc to cancel")
            color: "#6a6a72"
            font.pixelSize: 12
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 880
    height: 680
    minimumWidth: 560
    minimumHeight: 460
    visible: true
    title: qsTr("Defaults")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property bool typesView: tabs.currentIndex === 1
    readonly property var activeList: typesView ? typeList : categoryList

    // Types matching the search, by MIME type, description, or extension.
    readonly property var filteredTypes: {
        var query = search.text.trim().toLowerCase()
        var all = backend.types
        if (!query) return all
        var out = []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i]
            if (row.mime.toLowerCase().indexOf(query) !== -1
                    || row.description.toLowerCase().indexOf(query) !== -1
                    || row.patterns.toLowerCase().indexOf(query) !== -1
                    || (row.appName && row.appName.toLowerCase().indexOf(query) !== -1))
                out.push(row)
        }
        return out
    }

    function chooseCurrent() {
        if (picker.opened || help.opened) return
        if (typesView) {
            var type = filteredTypes[typeList.currentIndex]
            if (!type) return
            if (type.locked) {
                status.flash(qsTr("%1 is set in %2, which Omarchy manages. Change it there.").arg(type.mime).arg(type.lockedBy))
                return
            }
            picker.show("type", type.mime, type.description)
        } else {
            var category = backend.categories[categoryList.currentIndex]
            if (category) picker.show("category", category.id, category.title.toLowerCase())
        }
    }

    function focusList() {
        activeList.forceActiveFocus()
    }

    Shortcut { sequences: ["?", "Shift+?"]; context: Qt.ApplicationShortcut; onActivated: help.opened ? help.close() : help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !search.activeFocus; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !search.activeFocus; onActivated: win.chooseCurrent() }
    Shortcut { sequence: "Tab"; context: Qt.ApplicationShortcut; enabled: !picker.opened
        onActivated: { tabs.currentIndex = 1 - tabs.currentIndex; win.focusList() } }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: !search.activeFocus && !picker.opened
        onActivated: { tabs.currentIndex = 1; search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !search.activeFocus; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: !search.activeFocus; onActivated: backend.redo() }
    Shortcut { sequence: "F5"; context: Qt.ApplicationShortcut; onActivated: backend.reload() }

    Component.onCompleted: categoryList.forceActiveFocus()

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                spacing: 16

                ColumnLayout {
                    spacing: 2
                    Label {
                        text: qsTr("Defaults")
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        color: "#f2f2f2"
                    }
                    Label {
                        text: qsTr("Choose which app opens each kind of file.")
                        color: "#8a8a92"
                    }
                }
                Item { Layout.fillWidth: true }
                TabBar {
                    id: tabs
                    Material.background: "transparent"
                    TabButton { text: qsTr("Categories"); width: implicitWidth; onClicked: categoryList.forceActiveFocus() }
                    TabButton { text: qsTr("All file types"); width: implicitWidth; onClicked: typeList.forceActiveFocus() }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 12
                currentIndex: tabs.currentIndex

                // Categories
                ColumnLayout {
                    spacing: 0

                    ListView {
                        id: categoryList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: backend.categories
                        keyNavigationEnabled: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        Keys.onReturnPressed: win.chooseCurrent()
                        Keys.onEnterPressed: win.chooseCurrent()

                        delegate: ItemDelegate {
                            id: categoryRow
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 64
                            leftPadding: 24
                            rightPadding: 24
                            highlighted: ListView.isCurrentItem && categoryList.activeFocus
                            Material.roundedScale: Math.min(4, theme.radius)
                            onClicked: { categoryList.currentIndex = index; win.chooseCurrent() }

                            contentItem: RowLayout {
                                spacing: 16
                                AppIcon { name: categoryRow.modelData.icon; size: 32 }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: categoryRow.modelData.title
                                        color: "#f2f2f2"
                                        font.pixelSize: 15
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        color: "#8a8a92"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        text: {
                                            var row = categoryRow.modelData
                                            if (row.locked) return qsTr("Managed by Omarchy")
                                            if (row.otherApps > 0) return qsTr("Some types open with %n other apps", "", row.otherApps)
                                            if (row.appId && row.unset > 0) return qsTr("%n types have no default", "", row.unset)
                                            return qsTr("%n types", "", row.typeCount)
                                        }
                                    }
                                }
                                AppIcon {
                                    visible: !!categoryRow.modelData.appId
                                    name: categoryRow.modelData.appIcon
                                    size: 24
                                }
                                Label {
                                    Layout.maximumWidth: 260
                                    text: !categoryRow.modelData.appId
                                          ? (categoryRow.modelData.candidates > 0 ? qsTr("Not set") : qsTr("No app installed"))
                                          : categoryRow.modelData.automatic ? qsTr("%1 (automatic)").arg(categoryRow.modelData.appName)
                                          : categoryRow.modelData.appName
                                    color: categoryRow.modelData.appId ? "#e6e6e6" : "#6a6a72"
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        footer: Item {
                            width: categoryList.width
                            height: backend.omarchyAvailable() ? 64 : 0
                            visible: backend.omarchyAvailable()
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 36
                                anchors.rightMargin: 24
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Browser, editor and terminal are set in Omarchy's menu.")
                                    color: "#8a8a92"
                                    wrapMode: Text.WordWrap
                                }
                                Button {
                                    text: qsTr("Open Setup › Defaults")
                                    flat: true
                                    Material.roundedScale: Math.min(4, theme.radius)
                                    onClicked: backend.openOmarchyDefaults()
                                }
                            }
                        }
                    }
                }

                // All file types
                ColumnLayout {
                    spacing: 8

                    TextField {
                        id: search
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        placeholderText: qsTr("Search by type, extension or app")
                        Material.roundedScale: Math.min(4, theme.radius)
                        onTextChanged: typeList.currentIndex = 0
                        Keys.onDownPressed: { typeList.incrementCurrentIndex(); typeList.forceActiveFocus() }
                        Keys.onReturnPressed: { typeList.forceActiveFocus(); win.chooseCurrent() }
                        Keys.onEscapePressed: { if (text) text = ""; else typeList.forceActiveFocus() }
                    }

                    ListView {
                        id: typeList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: win.filteredTypes
                        keyNavigationEnabled: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        Keys.onReturnPressed: win.chooseCurrent()
                        Keys.onEnterPressed: win.chooseCurrent()
                        Keys.onUpPressed: function(event) {
                            if (currentIndex === 0) search.forceActiveFocus()
                            else decrementCurrentIndex()
                        }

                        delegate: ItemDelegate {
                            id: typeRow
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 56
                            leftPadding: 24
                            rightPadding: 24
                            highlighted: ListView.isCurrentItem && typeList.activeFocus
                            Material.roundedScale: Math.min(4, theme.radius)
                            onClicked: { typeList.currentIndex = index; typeList.forceActiveFocus(); win.chooseCurrent() }

                            contentItem: RowLayout {
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: typeRow.modelData.description
                                        color: "#f2f2f2"
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: text !== ""
                                        text: {
                                            var row = typeRow.modelData
                                            var parts = []
                                            if (row.description !== row.mime) parts.push(row.mime)
                                            if (row.patterns) parts.push(row.patterns)
                                            return parts.join("  ·  ")
                                        }
                                        color: "#6a6a72"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                                AppIcon {
                                    visible: !!typeRow.modelData.appId
                                    name: typeRow.modelData.appIcon
                                    size: 22
                                }
                                Label {
                                    Layout.maximumWidth: 240
                                    text: typeRow.modelData.locked ? qsTr("%1 (Omarchy)").arg(typeRow.modelData.appName)
                                          : !typeRow.modelData.appId ? qsTr("Not set")
                                          : typeRow.modelData.automatic ? qsTr("%1 (automatic)").arg(typeRow.modelData.appName)
                                          : typeRow.modelData.appName
                                    color: typeRow.modelData.appId ? "#e6e6e6" : "#6a6a72"
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: typeList.count === 0
                            text: qsTr("No file types match “%1”.").arg(search.text)
                            color: "#8a8a92"
                        }
                    }
                }
            }

            // What the last action did, or what went wrong.
            Rectangle {
                id: status
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: "#121215"

                property string message: backend.status
                property bool error: backend.statusIsError
                function flash(text) { message = text; error = true }
                Connections {
                    target: backend
                    function onStatusChanged() { status.message = backend.status; status.error = backend.statusIsError }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    Label {
                        Layout.fillWidth: true
                        text: status.message
                        color: status.error ? "#ff6b6b" : "#c8c8cc"
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("? for shortcuts")
                        color: "#6a6a72"
                    }
                }
            }
        }
    }

    AppPicker {
        id: picker
        onChosen: function(appId) {
            if (kind === "category") backend.setCategory(target, appId)
            else backend.setType(target, appId)
        }
        onClosed: win.focusList()
    }

    HelpOverlay { id: help; onClosed: win.focusList() }
}

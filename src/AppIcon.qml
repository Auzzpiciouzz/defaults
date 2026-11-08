import QtQuick

// An icon from the desktop's icon theme, by name or absolute path. Wrapped in
// an Item because layouts size an Image by its source, not by what we ask for.
Item {
    property string name: ""
    property int size: 32

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.fill: parent
        sourceSize: Qt.size(parent.size * 2, parent.size * 2)
        source: parent.name ? "image://icon/" + encodeURIComponent(parent.name) : ""
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
    }
}

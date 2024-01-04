import QtQuick
import Qt.labs.platform as QtLabs
import org.seatronomy.nautograf

Item {
    id: root

    property string title
    property bool useXdgFileDialog: false
    property variant parentWindow: undefined
    property string folder

    signal accepted
    signal error(var message)

    visible: false

    Connections {
        target: loader.item
        ignoreUnknownSignals: true

        function onAccepted() {
            root.folder = loader.item.folder;
            root.accepted();
        }

        function onVisibleChanged() {
            root.visible = loader.item.visible;
        }

        function onErrorOccurred() {
            root.error("Please ensure that xdg-desktop-portal-gnome or other portal implementation is installed.");
        }
    }

    Loader {
        id: loader

        onLoaded: {
            if (root.useXdgFileDialog) {
                item.visible = Qt.binding(function() { return root.visible })
                item.folder = root.folder;
                item.title = root.title;
                item.parentWindow = parentWindow;
            }
        }

        source: useXdgFileDialog ? "XdgFileDialogWrapper.qml" : ""
        sourceComponent: !useXdgFileDialog ? labsFileDialog : undefined
    }

    Component {
        id: labsFileDialog

        QtLabs.FolderDialog {
            currentFolder: root.folder
            title: root.title
            visible: root.visible
        }
    }
}

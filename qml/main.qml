import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Universal
import org.seatronomy.nautograf

ApplicationWindow {
    id: root
    visible: true
    title: "Nautograf"
    x: UserSettings.geometry.x
    y: UserSettings.geometry.y
    width: UserSettings.geometry.width
    height: UserSettings.geometry.height
    visibility: UserSettings.windowState
    Universal.theme: Universal.System
    footer: StatusBar {
        id: statusBar

        serverError: ChartModel.serverError
        catalogPath: ChartModel.dir
        catalogStatus: ChartModel.catalogType
        pixelsPerLongitude: viewer.pixelsPerLon

        onOpenCatalogSelector: folderDialog.visible = true
    }

    onVisibilityChanged: function(visibility) {
        UserSettings.windowState = visibility;
    }

    property int nonFullScrenMode: Window.Windowed
    property bool newCatalogChosen: false

    onXChanged: saveGeometry();
    onYChanged: saveGeometry();
    onWidthChanged: saveGeometry();
    onHeightChanged: saveGeometry();

    function saveGeometry() {
        UserSettings.geometry = Qt.rect(x, y, width, height);
    }

    Dialog {
        id: folderDialogErrorDialog

        title: qsTr("Failed to open folder dialog")
        anchors.centerIn: parent
        width: 600

        modal: true
        standardButtons: Dialog.Close

        Label {
            id: errorMessageLabel

            anchors.fill: parent
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Please choose a chart directory"

        onError: function(message) {
            errorMessageLabel.text = message;
            folderDialogErrorDialog.open();
        }

        onAccepted: function () {
            viewer.centerOnCatalogExtentWhenLoaded = true;
            ChartModel.setUrl(folder);
        }

        useXdgFileDialog: UseXdgFileDialog
        folder: "file:///" + ChartModel.dir
        parentWindow: root
    }

    function toggleFullscreen() {
        if (root.visibility != Window.FullScreen) {
            root.nonFullScrenMode = root.visibility;
            root.visibility = Window.FullScreen;
        } else {
            root.visibility = root.nonFullScrenMode;
        }
    }

    Item {
        anchors.fill: parent

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_F) {
                root.toggleFullscreen();
                event.accepted = true;
            }
        }

        Keys.onEscapePressed: function(event) {
            if (chartList.enabled || tileInfoLoader.enabled) {
                chartList.enabled = false;
                tileInfoLoader.enabled = false;
                viewer.focus = true;
            } else if (root.visibility == Window.FullScreen) {
                toggleFullscreen();
                event.accepted = true;
            }
        }

        Viewer {
            id: viewer

            focus: true
            highlightedTileVisible: tileInfoLoader.enabled
            anchors.fill: parent
            showLegacyRenderer: UserSettings.showLegacyRenderer
            showLegacyDebugView: UserSettings.showLegacyDebugView

            onSelectedTileRefChanged: {
                if (selectedTileRef.hasOwnProperty("tileId")) {
                    tileInfoLoader.selectTile(selectedTileRef);
                }
            }

            onShowContextMenu: function(x, y) {
                menu.open(x, y);
            }

            onNewMousePos: function(lat, lon) {
                statusBar.mouseLat = lat;
                statusBar.mouseLon = lon;
            }
        }

        ViewerMenu {
            id: menu

            onVisibleChanged: {
                if (!visible) {
                    viewer.focus = true;
                }
            }

            ViewerMenuItem {
                text: qsTr("Select chart dir...")
                onTriggered: folderDialog.visible = true
            }

            ViewerMenuItem {
                text: qsTr("Show chart selector")
                checkable: true
                checked: chartList.enabled
                onTriggered: chartList.enabled = !chartList.enabled
            }

            ViewerMenuItem {
                text: qsTr("Tile info")
                checkable: true
                checked: tileInfoLoader.enabled
                onTriggered: tileInfoLoader.enabled = !tileInfoLoader.enabled
            }

            ViewerMenuItem {
                text: qsTr("Fullscreen")
                checkable: true
                checked: root.visibility == Window.FullScreen;
                onTriggered: toggleFullscreen();
            }

            ViewerMenuItem {
                text: qsTr("Legacy renderer")
                checkable: true
                checked: UserSettings.showLegacyRenderer
                onTriggered: UserSettings.showLegacyRenderer = !UserSettings.showLegacyRenderer
            }

            ViewerMenuItem {
                text: qsTr("Legacy debug view")
                checkable: true
                enabled: UserSettings.showLegacyRenderer
                checked: UserSettings.showLegacyDebugView
                onTriggered: UserSettings.showLegacyDebugView = !UserSettings.showLegacyDebugView
            }

            ViewerMenuItem {
                text: qsTr("About")
                onTriggered: aboutLoader.active = true
            }

            ViewerMenuItem {
                text: qsTr("Close")
                onTriggered: Qt.quit()
            }
        }

        Loader {
            id: tileInfoLoader

            property variant initialTileRef: ({})
            property bool enabled: false

            function selectTile(tileRef) {
                enabled = true;
                if (status === Loader.Ready) {
                    item.tileRef = tileRef;
                } else {
                    initialTileRef = tileRef;
                }
            }

            onStatusChanged: {
                if (status === Loader.Ready) {
                    item.tileRef = initialTileRef;
                }
            }

            active: x > -width
            width: 400
            height: Math.max(600, parent.height - 100)
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.left
            }

            states: State {
                when: tileInfoLoader.enabled

                AnchorChanges {
                    target: tileInfoLoader
                    anchors {
                        right: undefined
                        left: parent.left
                    }
                }
            }

            transitions: Transition {
                AnchorAnimation {
                    duration: 200
                    easing.type: Easing.InOutQuad
                }
            }

            sourceComponent: Component {
                TileInfo {
                    id: tileInfo

                    focus: true
                    onSelectedTileChanged: viewer.highlightedTile = selectedTile
                    onClose: tileInfoLoader.enabled = false
                    anchors.fill: parent
                }
            }
        }

        Pane {
            id: chartList

            property bool enabled: false

            visible: x < root.width
            width: 400
            height: Math.max(600, parent.height - 100)
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.right
            }

            states: State {
                when: chartList.enabled

                AnchorChanges {
                    target: chartList
                    anchors {
                        left: undefined
                        right: parent.right
                    }
                }
            }

            transitions: Transition {
                AnchorAnimation {
                    duration: 200
                    easing.type: Easing.InOutQuad
                }
            }

            Button {
                text: "✕"
                anchors.top: parent.top
                anchors.left: parent.left
                onClicked: chartList.enabled = false;
            }

            ChartSelector {
                anchors {
                    margins: 20
                    topMargin: 40
                    fill: parent
                }
                model: ChartModel
            }
        }
    }

    Pane {
        anchors.centerIn: parent
        width: 700
        height: 100
        visible: ChartModel.loadingProgress < 1.0

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 5

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Loading charts...")
            }

            ProgressBar {
                Layout.fillWidth: true
                value: ChartModel.loadingProgress
            }
        }
    }

    Loader {
        id: aboutLoader

        active: false
        sourceComponent: About {
            onRejected: aboutLoader.active = false
        }
    }
}

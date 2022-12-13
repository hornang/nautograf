import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Universal
import Qt.labs.platform
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

    onVisibilityChanged: function(visibility) {
        UserSettings.windowState = visibility;
    }
    property int nonFullScrenMode: Window.Windowed

    onXChanged: saveGeometry();
    onYChanged: saveGeometry();
    onWidthChanged: saveGeometry();
    onHeightChanged: saveGeometry();

    function saveGeometry() {
        UserSettings.geometry = Qt.rect(x, y, width, height);
    }

    FolderDialog {
        id: folderDialog
        title: "Please choose a chart directory"
        onAccepted: function () {
            ChartModel.setUrl(folder);
        }
    }

    function toggleFullscreen() {
        if (root.visibility != Window.FullScreen) {
            root.nonFullScrenMode = root.visibility;
            root.visibility = Window.FullScreen;
        } else {
            root.visibility = root.nonFullScrenMode;
        }
    }

    Viewer {
        id: viewer

        focus: true
        showLegacyRenderer: UserSettings.showLegacyRenderer
        showLegacyDebugView: UserSettings.showLegacyDebugView

        Keys.onPressed: (event)=> {
            if (event.key === Qt.Key_Plus) {
                viewer.zoomIn();
            } else if (event.key === Qt.Key_Minus) {
                viewer.zoomOut();
            } else if (event.key === Qt.Key_F) {
                root.toggleFullscreen();
            } else if (event.key === Qt.Key_Escape) {
                if (root.visibility == Window.FullScreen) {
                    root.visibility = Window.Windowed;
                }
            }
        }

        Menu {
            id: menu

            MenuItem {
                text: qsTr("Select chart dir...")
                onTriggered: folderDialog.visible = true
            }

            MenuItem {
                text: qsTr("Show chart selector")
                onTriggered: drawer.open()
            }

            MenuItem {
                text: qsTr("Fullscreen")
                checkable: true
                checked: root.visibility == Window.FullScreen;
                onTriggered: toggleFullscreen();
            }

            Menu {
                title: qsTr("Legacy renderer")

                MenuItem {
                    text: qsTr("Enable")
                    checkable: true
                    checked: UserSettings.showLegacyRenderer
                    onTriggered: UserSettings.showLegacyRenderer = !UserSettings.showLegacyRenderer
                }

                MenuItem {
                    text: qsTr("Debug view")
                    checkable: true
                    enabled: UserSettings.showLegacyRenderer
                    checked: UserSettings.showLegacyDebugView
                    onTriggered: UserSettings.showLegacyDebugView = !UserSettings.showLegacyDebugView
                }
            }

            MenuItem {
                text: qsTr("About")
                onTriggered: aboutLoader.active = true
            }

            MenuItem {
                text: qsTr("Close")
                onTriggered: Qt.quit()
            }
        }

        anchors.fill: parent
        onShowContextMenu: function() {
            menu.open();
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

    Drawer {
        id: drawer
        width: 400
        height: root.height
        edge: Qt.RightEdge
        onClosed: ChartModel.writeVisibleCharts()

        ChartSelector {
            anchors.fill: parent
            anchors.margins: 20
            model: ChartModel
        }
    }

    Loader {
        id: aboutLoader
        active: false
        anchors.centerIn: parent
        sourceComponent: Component {
            Item {
                Rectangle {
                    color: "#fff"
                    opacity: 0.8
                    border.width: 1
                    radius: 5
                    border.color: "#666"
                    anchors.margins: -20
                    anchors.fill: aboutText

                    Button {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        flat: true
                        onClicked: aboutLoader.active = false
                        text: "❌"
                    }
                }

                TextEdit {
                    id: aboutText
                    wrapMode: Text.WordWrap
                    textFormat: Text.MarkdownText
                    selectByMouse: true
                    width: 550
                    anchors.centerIn: parent
                    onLinkActivated: function(url) {
                        Qt.openUrlExternally(url);
                    }
                    text: AboutText
                }
            }
        }
    }
}

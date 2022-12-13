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

    Item {
        anchors.fill: parent

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_F) {
                root.toggleFullscreen();
                event.accepted = true;
            }
        }

        Keys.onEscapePressed: function(event) {
            if (root.visibility == Window.FullScreen) {
                toggleFullscreen();
                event.accepted = true;
            }
        }

        Viewer {
            id: viewer

            focus: true
            anchors.fill: parent
            showLegacyRenderer: UserSettings.showLegacyRenderer
            showLegacyDebugView: UserSettings.showLegacyDebugView

            onShowContextMenu: function() {
                menu.open();
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
                checkable: true
                checked: chartList.enabled
                onTriggered: chartList.enabled = !chartList.enabled
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

        Pane {
            id: chartList

            property bool enabled: false

            visible: x < root.width
            width: 400
            anchors {
                top: parent.top
                topMargin: 80
                bottom: parent.bottom
                bottomMargin: 80
                left: parent.right
            }

            onEnabledChanged: {
                if (enabled) {
                    chartList.focus = true;
                } else {
                    viewer.focus = true;
                }
            }

            Keys.onEscapePressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    enabled = false;
                    event.accepted = true;
                }
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

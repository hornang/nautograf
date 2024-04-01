import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Universal 2.12
import org.seatronomy.nautograf 1.0

Pane {
    id: root

    property alias chartNames: repeater.model
    property alias selectedTile: tileInfoBackend.tileId
    property variant tileRef

    signal tileSelected(var tileId)
    signal close

    function selectTile(tileRef) {
        tileInfoBackend.tileRef = tileRef;
    }

    onTileRefChanged: selectTile(tileRef)

    Keys.onEscapePressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.close();
            event.accepted = true;
        }
    }


    TileInfoBackend {
        id: tileInfoBackend

        tileFactory: TileFactory
    }

    Button {
        text: "✕"
        anchors.top: parent.top
        anchors.right: parent.right
        onClicked: root.close()
    }

    SplitView {
        orientation: Qt.Vertical
        anchors.fill: parent
        anchors.topMargin: 50

        ListView {
            id: tileList

            model: MapTileModel
            SplitView.preferredHeight: 400
            spacing: 2

            delegate: Pane {
                height: 40
                padding: 0
                width: tileList.width

                RowLayout {
                    anchors.fill: parent

                    MouseArea {
                        Layout.fillHeight: true
                        Layout.fillWidth: true

                        onClicked: {
                            if (model.tileRef.tileId === tileInfoBackend.tileId) {
                                root.selectTile({});
                                root.tileSelected("");
                                tileList.currentIndex = -1;
                            } else {
                                root.selectTile(model.tileRef);
                                root.tileSelected(model.tileRef.tileId);
                                tileList.currentIndex = index;
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: model.tileId === tileInfoBackend.tileId
                            color: Universal.accent
                        }

                        Label {
                            anchors {
                                verticalCenter: parent.verticalCenter
                                left: parent.left
                                margins: 5
                            }
                            text: model.tileId
                            font.family: "Lucida Console"
                        }

                    }

                    Button {
                        icon.name: CanUseIconTheme ? "edit-copy" : ""
                        text: !CanUseIconTheme ? "📋" : ""
                    }
                }

            }
        }

        ColumnLayout {
            TabBar {
                id: bar

                Layout.fillWidth: true

                TabButton {
                    text: qsTr("Charts")
                }
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: bar.currentIndex

                ListView {
                    id: repeater

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: tileInfoBackend.chartInfos

                    delegate: CheckBox {
                        id: checkbox

                        font.family: "Lucida Console"
                        checkable: true
                        checked: modelData["enabled"]
                        onClicked: tileInfoBackend.setChartEnabled(modelData["name"], checked);
                        text: modelData["name"]
                    }
                }
            }
        }
    }
}

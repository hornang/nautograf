import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Universal
import org.seatronomy.nautograf 1.0

Item {
    id: root

    property real lat: UserSettings.lat
    property real lon: UserSettings.lon
    property real pixelsPerLon: UserSettings.pixelsPerLon

    signal showContextMenu(var x, var y)

    property string highlightedTile
    property bool highlightedTileVisible
    property var selectedTileRef: ({})
    property bool showLegacyRenderer: false
    property bool showLegacyDebugView: false

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Plus) {
            zoomIn();
            event.accepted = true;
        } else if (event.key === Qt.Key_Minus) {
            zoomOut();
            event.accepted = true;
        }
    }

    function updateTileModel() {
        MapTileModel.setPanZoom(Qt.point(root.lon, root.lat), root.pixelsPerLon);
        UserSettings.lat = root.lat;
        UserSettings.lon = root.lon;
        UserSettings.pixelsPerLon = root.pixelsPerLon;
    }

    function zoomIn() {
        const pixelsPerLatitude = root.pixelsPerLon * 1.5;
        adjustZoom(pixelsPerLatitude, Qt.point(mapRoot.width / 2, mapRoot.height / 2), Qt.point(0, 0));
    }

    function zoomOut() {
        const pixelsPerLatitude = root.pixelsPerLon / 1.5;
        adjustZoom(pixelsPerLatitude, Qt.point(mapRoot.width / 2, mapRoot.height / 2), Qt.point(0, 0));
    }

    Component.onCompleted: updateTileModel();
    onLatChanged: updateTileModel()
    onLonChanged: updateTileModel()
    onPixelsPerLonChanged: updateTileModel()

    Rectangle {
        id: mapRoot
        anchors.fill:parent
        color: "#eee"

        onWidthChanged: updateViewport()
        onHeightChanged: updateViewport()

        function updateViewport() {
            if (mapRoot.width > 0 && mapRoot.height > 0) {
                MapTileModel.setViewPort(Qt.size(mapRoot.width, mapRoot.height))
            }
        }

        PinchArea {
            id: pinchArea
            property point startPos
            property int startPixelsPerLongitude

            anchors.fill: parent
            onPinchStarted: function (event){
                startPos = Qt.point(root.lon, root.lat);
                startPixelsPerLongitude = root.pixelsPerLon;

            }

            onPinchUpdated: function (event) {
                var offset = Qt.point(event.center.x - event.previousCenter.x,
                                      event.center.y - event.previousCenter.y);
                adjustZoom(startPixelsPerLongitude * event.scale,
                           Qt.point(event.center.x, event.center.y),
                           offset);
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                property point mouseStartPos
                property point startPos
                hoverEnabled: true

                onPressAndHold: function(mouse) {
                    const distance = Math.sqrt(Math.pow(mouse.x - mouseStartPos.x, 2) +
                                               Math.pow(mouse.y - mouseStartPos.y, 2));
                    if (distance < 10) {
                        root.showContextMenu(mouse.x, mouse.y);
                    }
                }

                onDoubleClicked: function (mouse) {
                    const topLeft = Qt.point(root.lon, root.lat);
                    const position = mapTile.offsetPosition(topLeft,
                                                            root.pixelsPerLon,
                                                            Qt.point(mouse.x, mouse.y));
                    const tileRef = MapTileModel.tileRefAtPos(position.y, position.x);
                    root.selectedTileRef = tileRef;
                }

                onPressed: function (mouse) {
                    startPos = Qt.point(root.lon, root.lat);
                    mouseStartPos = Qt.point(mouse.x, mouse.y);
                    mouse.accept = false;
                    statusBar.position = mapTile.offsetPosition(startPos,
                                                                root.pixelsPerLon,
                                                                Qt.point(mouse.x, mouse.y));
                }

                MapTile {
                    // A dummy map tile used for some computations
                    id: mapTile
                }

                onPositionChanged: function (mouse) {
                    if (pressed) {
                        let offset = mapTile.offsetPosition(mouseArea.startPos,
                                                            root.pixelsPerLon,
                                                            Qt.point(mouseArea.mouseStartPos.x - mouse.x,
                                                                     mouseArea.mouseStartPos.y - mouse.y));
                        root.lon = offset.x;
                        root.lat = offset.y;
                    } else {
                        const topLeft = Qt.point(root.lon, root.lat);
                        const position = mapTile.offsetPosition(topLeft,
                                                                root.pixelsPerLon,
                                                                Qt.point(mouse.x, mouse.y));
                        statusBar.position = position;
                    }
                }

                onWheel: function (wheel) {
                    let zoomFactor = Math.pow(2, wheel.angleDelta.y / 500);
                    adjustZoom(root.pixelsPerLon * zoomFactor,
                               Qt.point(wheel.x, wheel.y),
                               Qt.point(0, 0));
                }
            }
        }

        Repeater {
            id: legacyRenderer

            model: root.showLegacyRenderer ? MapTileModel : null

            MapTile {
                id: tile

                lat: root.lat
                lon: root.lon
                pixelsPerLon: root.pixelsPerLon
                tileFactory: TileFactory
                tileRef: model.tileRef

                onLoadingChanged: function(loading) {
                    if (loading) {
                        timer.start();
                    }
                }

                Timer {
                    id: timer
                    interval: 500
                }

                BusyIndicator {
                    id: busyIndicator
                    anchors.centerIn: parent
                    running: tile.loading && !timer.running
                }

                Item {
                    id: overlayContent
                    anchors.fill: parent
                    anchors.margins: tile.margin

                    Rectangle {
                        visible: busyIndicator.running || root.showLegacyDebugView || tile.noData || tile.error
                        anchors.fill: parent
                        border {
                            width: 1
                            color: "#ccc"
                        }
                        color: "transparent"
                        Text {
                            visible: tile.error || tile.noData
                            anchors.centerIn: parent
                            text: tile.error ? "⚠Render thread crashed" : "🔍No data"
                        }
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.showLegacyDebugView
                        sourceComponent: Component {
                            Item {
                                anchors.fill: parent
                                Rectangle {
                                    color: "#88ffffff"
                                    radius: 2
                                    border.width: 1
                                    border.color: "#77000000"
                                    width: textEdit.width + 10
                                    height: textEdit.height + 10
                                    anchors {
                                        top: parent.top
                                        right: parent.right
                                        margins: 20
                                    }

                                    TextEdit {
                                        id: textEdit
                                        anchors.centerIn: parent
                                        font.pixelSize: 20
                                        readOnly: true
                                        font.family: "fixed"
                                        selectByMouse: true
                                        text: model.tileRef["tileId"]
                                    }
                                }

                                Rectangle {
                                    anchors {
                                        topMargin: 20
                                        leftMargin: 20
                                        top: parent.top
                                        left: parent.left
                                    }
                                    width: column.width + 20
                                    height: column.height + 20
                                    color: "#88ffffff"
                                    border.width: 1
                                    border.color: "#77000000"
                                    radius: 2
                                    visible: tile.charts.length

                                    Column {
                                        id: column
                                        spacing: 0
                                        anchors {
                                            left: parent.left
                                            top: parent.top
                                            margins: 10
                                        }

                                        Repeater {
                                            model: tile.charts
                                            CheckBox {
                                                id: checkbox
                                                checkable: true
                                                checked: tile.chartVisible(modelData) // Initial value
                                                onCheckedChanged: function (checked) {
                                                    tile.setChartVisibility(modelData, checkbox.checked)
                                                }
                                                text: modelData
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Loader {
            anchors.fill: parent
            active: !root.showLegacyRenderer
            sourceComponent: Scene {
                id: sceneGraphRenderer

                focusedTile: root.highlightedTileVisible ? root.highlightedTile : ""
                accentColor: Universal.accent
                tileFactory: TileFactory
                lat: root.lat
                lon: root.lon
                pixelsPerLon: root.pixelsPerLon
                tileModel: MapTileModel
            }
        }
    }

    StatusBar {
        id: statusBar
        cryptReaderStatus: ChartModel.cryptReaderStatus
        pixelsPerLongitude: root.pixelsPerLon
        anchors {
            bottom: parent.bottom
            right: parent.right
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.showContextMenu(mouse.x, mouse.y)
        acceptedButtons: Qt.RightButton
    }

    function adjustZoom(newPixelsPerLongitude, zoomOrigin, offset) {
        const topLeft = Qt.point(root.lon, root.lat);
        let lockPosition = mapTile.offsetPosition(topLeft,
                                                  root.pixelsPerLon,
                                                  zoomOrigin);

        newPixelsPerLongitude = Math.min(Math.max(newPixelsPerLongitude, 50), 100000);

        const newTopLeft = mapTile.offsetPosition(lockPosition,
                                                  newPixelsPerLongitude,
                                                  Qt.point(-zoomOrigin.x - offset.x,
                                                           -zoomOrigin.y - offset.y));
        root.lon = newTopLeft.x;
        root.lat = newTopLeft.y;
        root.pixelsPerLon = newPixelsPerLongitude;
    }
}

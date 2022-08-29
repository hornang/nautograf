import QtQuick
import QtQuick.Window
import QtQuick.Controls
import org.seatronomy.nautograf 1.0

Item {
    id: root
    property vector3d viewport: UserSettings.viewport

    signal showContextMenu(var x, var y)
    signal toggleFullscreen
    signal exitFullscreen
    property bool debugInfo: UserSettings.debugView

    function updateTileModel() {
        MapTileModel.setPanZoom(Qt.point(root.viewport.x, root.viewport.y), root.viewport.z);
        UserSettings.viewport = root.viewport;
    }

    Component.onCompleted: updateTileModel();
    onViewportChanged: updateTileModel()
    focus: true

    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Plus) {
            const pixelsPerLatitude = viewport.z * 1.5;
            adjustZoom(pixelsPerLatitude, Qt.point(mapRoot.width / 2, mapRoot.height / 2), Qt.point(0, 0));
        } else if (event.key === Qt.Key_Minus) {
            const pixelsPerLatitude = viewport.z / 1.5;
            adjustZoom(pixelsPerLatitude, Qt.point(mapRoot.width / 2, mapRoot.height / 2), Qt.point(0, 0));
        } else if (event.key === Qt.Key_F) {
            root.toggleFullscreen();
        } else if (event.key === Qt.Key_Escape) {
            root.exitFullscreen();
        }
    }

    Rectangle {
        id: mapRoot
        anchors.fill:parent
        color: "#eee"
        property bool zoomAnimation: true

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
                startPos = Qt.point(root.viewport.x, root.viewport.y);
                mapRoot.zoomAnimation = false;
                startPixelsPerLongitude = root.viewport.z;

            }

            onPinchUpdated: function (event) {
                var offset = Qt.point(event.center.x - event.previousCenter.x,
                                      event.center.y - event.previousCenter.y);
                adjustZoom(startPixelsPerLongitude * event.scale,
                           Qt.point(event.center.x, event.center.y),
                           offset);
            }
            onPinchFinished: mapRoot.zoomAnimation = true;

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                property point mouseStartPos
                property point startPos
                hoverEnabled: true
                onDoubleClicked: function (mouse) {
                    root.showContextMenu(mouse.x, mouse.y);
                }

                onPressed: function (mouse) {
                    startPos = Qt.point(root.viewport.x, root.viewport.y);
                    mouseStartPos = Qt.point(mouse.x, mouse.y);
                    mouse.accept = false;
                    mapRoot.zoomAnimation = true;
                    statusBar.position = mapTile.offsetPosition(startPos, root.viewport.z, Qt.point(mouse.x, mouse.y));
                }

                MapTile {
                    // A dummy map tile used for some computations
                    id: mapTile
                }

                onPositionChanged: function (mouse) {
                    if (pressed) {
                        let offset = mapTile.offsetPosition(mouseArea.startPos,
                                                            root.viewport.z,
                                                            Qt.point(mouseArea.mouseStartPos.x - mouse.x,
                                                                     mouseArea.mouseStartPos.y - mouse.y));
                        root.viewport = Qt.vector3d(offset.x,
                                                    offset.y,
                                                    root.viewport.z)
                    } else {
                        const topLeft = Qt.point(root.viewport.x, root.viewport.y);
                        const position = mapTile.offsetPosition(topLeft,
                                                                root.viewport.z,
                                                                Qt.point(mouse.x, mouse.y));
                        statusBar.position = position;
                    }
                }

                onWheel: function (wheel) {
                    let zoomFactor;
                    if (wheel.angleDelta.y > 0) {
                        zoomFactor = 1.5;
                    } else {
                        zoomFactor = 1 / 1.5;
                    }
                    adjustZoom(root.viewport.z * zoomFactor, Qt.point(wheel.x, wheel.y), Qt.point(0, 0));
                }
            }
        }

        Repeater {
            model: MapTileModel

            MapTile {
                id: tile
                viewport: root.viewport
                tileFactory: TileFactory
                tileRef: model.tileRef
                symbols: ChartSymbols

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

                onNewGeometry: function (x_, y_, w, h) {
                    widthAnimation.to = w
                    heightAnimation.to = h
                    xAnimation.to = x_
                    yAnimation.to = y_

                    if (mapRoot.zoomAnimation) {
                        if (parallelAnimation.running) {
                            parallelAnimation.stop()
                        }
                        parallelAnimation.start();
                    } else {
                        tile.x = x_;
                        tile.y = y_;
                        tile.width = w;
                        tile.height = h;
                    }
                }

                ParallelAnimation {
                    id: parallelAnimation
                    PropertyAnimation {
                        id: xAnimation
                        target: tile
                        property: "x"
                        to: 50
                        easing.type: Easing.InOutQuad
                        duration: 200
                    }
                    PropertyAnimation {
                        id: yAnimation
                        target: tile
                        property: "y"
                        to: 50
                        easing.type: Easing.InOutQuad
                        duration: 200
                    }
                    PropertyAnimation {
                        id: widthAnimation
                        target: tile
                        property: "width"
                        to: 50
                        easing.type: Easing.InOutQuad
                        duration: 200
                    }
                    PropertyAnimation {
                        id: heightAnimation
                        target: tile
                        property: "height"
                        to: 50
                        easing.type: Easing.InOutQuad
                        duration: 200
                    }
                }

                Item {
                    id: overlayContent
                    anchors.fill: parent
                    anchors.margins: tile.margin

                    Rectangle {
                        visible: busyIndicator.running || root.debugInfo || tile.noData || tile.error
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
                        active: root.debugInfo
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
    }

    StatusBar {
        id: statusBar
        cryptReaderStatus: ChartModel.cryptReaderStatus
        pixelsPerLongitude: root.viewport.z
        anchors {
            bottom: parent.bottom
            right: parent.right
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: function(mouse) {
            root.showContextMenu(mouse.x, mouse.y);
        }
        acceptedButtons: Qt.RightButton
    }

    function adjustZoom(newPixelsPerLongitude, zoomOrigin, offset) {
        const topLeft = Qt.point(root.viewport.x, root.viewport.y);
        let lockPosition = mapTile.offsetPosition(topLeft,
                                                  root.viewport.z,
                                                  zoomOrigin);
        if (newPixelsPerLongitude > 80000) {
            newPixelsPerLongitude = 80000;
        } else if (newPixelsPerLongitude < 50) {
            newPixelsPerLongitude = 50;
        }

        const newTopLeft = mapTile.offsetPosition(lockPosition,
                                                  newPixelsPerLongitude,
                                                  Qt.point(-zoomOrigin.x - offset.x,
                                                           -zoomOrigin.y - offset.y));
        root.viewport = Qt.vector3d(newTopLeft.x,
                                    newTopLeft.y,
                                    newPixelsPerLongitude)
    }
}

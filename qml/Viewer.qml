import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Universal
import org.seatronomy.nautograf 1.0

Item {
    id: root

    property real centerLat: UserSettings.lat
    property real centerLon: UserSettings.lon
    property real pixelsPerLon: UserSettings.pixelsPerLon
    property bool centerOnCatalogExtentWhenLoaded: false

    signal showContextMenu(var x, var y)
    signal newMousePos(var lat, var lon)

    property string highlightedTile
    property bool highlightedTileVisible
    property var selectedTileRef: ({})
    property bool showLegacyRenderer: false
    property bool showLegacyDebugView: false
    readonly property real minPixelsPerLon: 50
    readonly property real maxPixelsPerLon: 100000

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
        MapTileModel.setPanZoom(root.centerLon, root.centerLat, root.pixelsPerLon);
        UserSettings.lat = root.centerLat;
        UserSettings.lon = root.centerLon;
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

    function centerViewOnExtent(top, bottom, left, right)  {
        const pixelsPerLonToFitWidth = Mercator.pixelsPerLonInterval(left, right, mapRoot.width);
        const pixelsPerLonToFitHeight = Mercator.pixelsPerLatInterval(top, bottom, mapRoot.height);
        const pixelsPerLon = Math.min(pixelsPerLonToFitWidth, pixelsPerLonToFitHeight);

        root.pixelsPerLon = Math.max(root.minPixelsPerLon, Math.min(root.maxPixelsPerLon, pixelsPerLon));
        root.centerLat = (top + bottom) / 2;
        root.centerLon = (left + right) / 2;
    }

    Component.onCompleted: updateTileModel();
    onCenterLatChanged: updateTileModel()
    onCenterLonChanged: updateTileModel()
    onPixelsPerLonChanged: updateTileModel()

    Connections {
        target: ChartModel

        function onCatalogExtentCalculated(top, bottom, left, right) {
            if (root.centerOnCatalogExtentWhenLoaded) {
                centerViewOnExtent(top, bottom, left, right);
                centerOnCatalogExtentWhenLoaded = false;
            }
        }
    }

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

            property int startPixelsPerLongitude

            anchors.fill: parent
            onPinchStarted: function (event){
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

                property point mouseStartPos
                property real mousePressLon
                property real mousePressLat

                anchors.fill: parent
                hoverEnabled: true

                onPressAndHold: function(mouse) {
                    const distance = Math.sqrt(Math.pow(mouse.x - mouseStartPos.x, 2) +
                                               Math.pow(mouse.y - mouseStartPos.y, 2));
                    if (distance < 10) {
                        root.showContextMenu(mouse.x, mouse.y);
                    }
                }

                onClicked: root.focus = true

                onDoubleClicked: function (mouse) {
                    const viewPortCenter = Qt.point(mapRoot.width / 2, mapRoot.height / 2);

                    const lon = Mercator.offsetLon(root.centerLon,
                                                   mouse.x - viewPortCenter.x,
                                                   root.pixelsPerLon);

                    const lat = Mercator.offsetLat(root.centerLat,
                                                   mouse.y - viewPortCenter.y,
                                                   root.pixelsPerLon);

                    const tileRef = MapTileModel.tileRefAtPos(lat, lon);
                    root.selectedTileRef = tileRef;
                }

                onPressed: function (mouse) {
                    mousePressLat = root.centerLat;
                    mousePressLon = root.centerLon;
                    mouseStartPos = Qt.point(mouse.x, mouse.y);
                    mouse.accept = false;

                    const viewPortCenter = Qt.point(mapRoot.width / 2, mapRoot.height / 2);

                    const lon = Mercator.offsetLon(root.centerLon,
                                                   mouse.x - viewPortCenter.x,
                                                   root.pixelsPerLon);

                    const lat = Mercator.offsetLat(root.centerLat,
                                                   mouse.y - viewPortCenter.y,
                                                   root.pixelsPerLon);

                    root.newMousePos(lon, lat);

                }

                onPositionChanged: function (mouse) {
                    if (pressed) {
                        root.centerLon = Mercator.offsetLon(mouseArea.mousePressLon,
                                                            mouseArea.mouseStartPos.x - mouse.x,
                                                            root.pixelsPerLon);

                        root.centerLat = Mercator.offsetLat(mouseArea.mousePressLat,
                                                            mouseArea.mouseStartPos.y - mouse.y,
                                                            root.pixelsPerLon);
                    } else {
                        const lon = Mercator.offsetLon(root.centerLon,
                                                       mouseArea.mouseStartPos.x - mouse.x,
                                                       root.pixelsPerLon);

                        const lat = Mercator.offsetLat(root.centerLat,
                                                       mouseArea.mouseStartPos.y - mouse.y,
                                                       root.pixelsPerLon);

                        root.newMousePos(lon, lat);
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

                lat: Mercator.offsetLat(root.centerLat, -mapRoot.height / 2, root.pixelsPerLon)
                lon: Mercator.offsetLon(root.centerLon, -mapRoot.width / 2, root.pixelsPerLon)
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
                lat: root.centerLat
                lon: root.centerLon
                pixelsPerLon: root.pixelsPerLon
                tileModel: MapTileModel
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton

        onClicked: function(mouse) {
            root.showContextMenu(mouse.x, mouse.y);
        }
    }

    function adjustZoom(newPixelsPerLongitude, zoomOrigin, offset) {
        const viewPortCenter = Qt.point(mapRoot.width / 2, mapRoot.height / 2);

        const lockedLon = Mercator.offsetLon(root.centerLon,
                                             zoomOrigin.x - viewPortCenter.x,
                                             root.pixelsPerLon);

        root.centerLon = Mercator.offsetLon(lockedLon,
                                            viewPortCenter.x - zoomOrigin.x - offset.x,
                                            newPixelsPerLongitude);


        const lockedLat = Mercator.offsetLat(root.centerLat,
                                             zoomOrigin.y - viewPortCenter.y,
                                             root.pixelsPerLon);

        root.centerLat = Mercator.offsetLat(lockedLat,
                                            viewPortCenter.y - zoomOrigin.y - offset.y,
                                            newPixelsPerLongitude);

        root.pixelsPerLon = newPixelsPerLongitude;
    }
}

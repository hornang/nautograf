import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Universal
import QtQuick.Controls

import org.seatronomy.nautograf

ToolBar {
    id: root

    property string catalogPath
    property variant catalogStatus
    property bool serverError: false
    property real mouseLat
    property real mouseLon
    property real pixelsPerLongitude: 1

    position: ToolBar.Footer

    signal openCatalogSelector

    RowLayout {
        id: row

        anchors.fill: parent

        ToolButton {
            text: "📁"

            onClicked: root.openCatalogSelector()
        }

        Label  {
            id: catalogPath

            elide: Text.ElideRight
            Layout.fillWidth: true
            text: root.catalogPath
        }

        Label {
            visible: root.catalogStatus !== ChartModel.NotLoaded
            text: {
                switch(root.catalogStatus) {
                case ChartModel.Oesu:
                    return qsTr("🔐 OESU");
                case ChartModel.Oesenc:
                    return qsTr("🔐 OESENC");
                case ChartModel.Unencrypted:
                    return qsTr("🔓 Unencrypted");
                case ChartModel.Invalid:
                    return qsTr("⛔ Invalid catalog");
                default:
                    return "";
                }
            }
        }

        Label {
            visible: root.serverError
            text: root.serverError ? qsTr("oexserverd unavailable") : ""
        }

        ToolSeparator {
        }

        Label {
            Layout.leftMargin: 20
            property real value: 52246 / (root.pixelsPerLongitude / 2560 * 0.6)
            property string pixelsPerLongitude: Number(root.pixelsPerLongitude).toLocaleString(Qt.locale(), 'f', 0)
            text: "1 : " + Number(value).toLocaleString(Qt.locale(), 'f', 0) + " (" + pixelsPerLongitude + ")"
        }

        Label {
            Layout.preferredWidth: 130
            text: Number(root.mouseLat).toLocaleString(Qt.locale(), 'f', 7)
        }

        Label {
            Layout.rightMargin: 20
            Layout.preferredWidth: 130
            text: Number(root.mouseLon).toLocaleString(Qt.locale(), 'f', 7)
        }
    }
}

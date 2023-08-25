import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Universal
import QtQuick.Controls

ToolBar {
    id: root

    property string cryptReaderStatus
    property real mouseLat
    property real mouseLon
    property real pixelsPerLongitude: 1

    position: ToolBar.Footer

    signal openCatalogueSelector

    RowLayout {
        id: row

        anchors.fill: parent

        Label  {
            id: errorText

            Layout.fillWidth: true
            Layout.leftMargin: 10
            text: "oexserverd: " + root.cryptReaderStatus
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

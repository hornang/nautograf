import QtQuick 2.12
import QtQuick.Layouts 1.3

Rectangle {
    id: root
    property real pixelsPerLongitude: 0
    property point position
    property string cryptReaderStatus
    color: "#00000000"
    radius: 0
    height: 40
    width: row.width

    RowLayout {
        id: row
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        spacing: 30

        OverlayText  {
            id: errorText
            Layout.rightMargin: 20
            text: "oexserverd: " + root.cryptReaderStatus
        }

        OverlayText {
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 20
            property real value: 52246 / (root.pixelsPerLongitude / 2560 * 0.6)
            property string pixelsPerLongitude: Number(root.pixelsPerLongitude).toLocaleString(Qt.locale(), 'f', 0)
            text: "1 : " + Number(value).toLocaleString(Qt.locale(), 'f', 0) + " (" + pixelsPerLongitude + ")"
        }

        OverlayText {
            Layout.alignment: Qt.AlignVCenter
            text: Number(root.position.y).toLocaleString(Qt.locale(), 'f', 7)
        }

        OverlayText{
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 20
            text: Number(root.position.x).toLocaleString(Qt.locale(), 'f', 7)
        }
    }
}

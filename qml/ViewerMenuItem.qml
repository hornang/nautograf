import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Universal

Item {
    id: root

    property bool checked
    property bool checkable
    property string text

    implicitWidth: layout.width
    implicitHeight: layout.height

    Layout.fillWidth: true

    signal triggered

    RowLayout {
        id: layout

        height: 50

        CheckBox {
            id: checkbox

            text: root.text
            checked: root.checked
            visible: root.checkable
        }

        Label {
            id: text

            text: root.text
            visible: !root.checkable
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.triggered()
    }
}



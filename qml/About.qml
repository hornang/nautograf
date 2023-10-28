import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Universal
import org.seatronomy.nautograf

Dialog {
    id: dialog

    anchors.centerIn: Overlay.overlay
    visible: true
    modal: true
    standardButtons: Dialog.Close
    background: Rectangle {
        property int offset: 3
        x: -offset
        y: -offset
        width: parent.width + offset * 2
        height: parent.height + offset * 2
        radius: offset * 3
        color: dialog.footer.background.color // How to avoid this hack?
    }

    header: ColumnLayout {
        Image {
            source: "graphics/title.svg"
            Layout.alignment: Qt.AlignCenter
            Layout.topMargin: 10
        }

        Label {
            text: "v" + AppVersion
            Layout.alignment: Qt.AlignCenter
        }

        TabBar {
            id: bar

            Layout.fillWidth: true

            TabButton {
                text: qsTr("About")
            }

            TabButton {
                text: qsTr("Licenses")
            }
        }
    }

    StackLayout {
        currentIndex: bar.currentIndex

        TextEdit {
            Layout.maximumWidth: 500
            Layout.minimumHeight: implicitHeight
            wrapMode: Text.WordWrap
            color: Universal.foreground
            textFormat: Text.MarkdownText
            selectByMouse: true
            onLinkActivated: function(url) {
                Qt.openUrlExternally(url);
            }
            text: AboutText
        }

        ColumnLayout  {
            id: licensesLayout

            Licenses {
                id: licenseModel

                index: comboBox.currentIndex
            }

            ComboBox {
                id: comboBox

                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                Layout.bottomMargin: 20
                model: licenseModel.names
            }

            Flickable {
                id: flickable

                visible: parent.visible
                clip: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 400
                Layout.preferredWidth: licenseText.width
                contentHeight: licenseText.height
                ScrollBar.vertical: ScrollBar {}

                Label {
                    id: licenseText

                    font.family: "fixed"
                    text: licenseModel.currentLicense
                }
            }
        }
    }
}

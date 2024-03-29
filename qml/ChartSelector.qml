import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Universal 2.12

ColumnLayout {
    id: root

    property alias model: listView.model

    CheckBox {
        text: "All"
        Layout.fillWidth: true
        nextCheckState: function() { return checkState }
        checked: root.model.allEnabled
        onClicked: root.model.setAllChartsEnabled(!checked)
    }

    ScrollView {
        clip: true
        Layout.fillHeight: true
        Layout.fillWidth: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ListView {
            id: listView
            delegate: RowLayout {
                width: listView.width
                CheckBox {
                    nextCheckState: function() { return checkState }
                    checked: model.enabled && model.ok
                    enabled: model.ok
                    onClicked: root.model.setChartEnabled(index, !checked)
                    font.family: "fixed"

                    property string symbol: {
                        if (!model.ok) {
                            return "❌";
                        } else {
                            return ""
                        }
                    }
                    text: symbol + model.name
                }
                Label {
                    visible: model.ok
                    text: "1:" + model.scale
                }
            }
        }
    }
}

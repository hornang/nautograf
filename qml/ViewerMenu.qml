import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Universal

Pane {
    id: root

    default property alias contents: layout.children

    visible: false

    padding: 20
    implicitWidth: layout.width + 2 * padding
    implicitHeight: layout.height + 2 * padding

    Keys.onEscapePressed: function(event) {
        if (visible) {
            visible = false;
            event.accepted = true;
        }
    }

    onFocusChanged: {
        if (!focus) {
            visible = false;
        }
    }

    function open(x, y) {
        root.x = x;
        root.y = y;

        visible = true;
        focus = true;
    }

    function childClicked() {
        visible = false;
    }

    ColumnLayout {
        id: layout

        anchors.centerIn: parent
        spacing: 10

        function listenToChildren() {
            for (var i = 0; i < children.length; i++) {
                children[i].triggered.connect(root.childClicked);
            }
        }

        onChildrenChanged: listenToChildren()
    }
}

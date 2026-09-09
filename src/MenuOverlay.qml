import QtQuick

// The pop-up menus behind MATH, LIST, DISTR, CALC, ZOOM and the rest. Each
// entry either types something into the current field or runs a command.
Item {
    id: overlay
    property var app
    property string title: ""
    property var entries: []

    function open(newTitle, items) {
        overlay.title = newTitle;
        overlay.entries = items;
        overlay.visible = true;
    }

    visible: false
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: app.pageColor
        opacity: 0.94
        MouseArea { anchors.fill: parent; onClicked: overlay.visible = false }
    }

    Column {
        anchors.fill: parent
        anchors.margins: Math.round(10 * app.uiScale)
        spacing: Math.round(6 * app.uiScale)

        Row {
            width: parent.width
            Text {
                text: overlay.title
                color: app.accentColor
                font.family: "iA Writer Mono S"
                font.bold: true
                font.pixelSize: Math.round(15 * app.uiScale)
            }
            Item { width: parent.width - Math.round(150 * app.uiScale); height: 1 }
            Text {
                text: "close ✕"
                color: app.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * app.uiScale)
                MouseArea { anchors.fill: parent; onClicked: overlay.visible = false }
            }
        }

        GridView {
            width: parent.width
            height: parent.height - Math.round(30 * app.uiScale)
            clip: true
            cellWidth: Math.round(width / Math.max(2, Math.floor(width / (110 * app.uiScale))))
            cellHeight: Math.round(30 * app.uiScale)
            model: overlay.entries

            delegate: Rectangle {
                width: GridView.view.cellWidth - Math.round(4 * app.uiScale)
                height: GridView.view.cellHeight - Math.round(4 * app.uiScale)
                radius: Math.round(4 * app.uiScale)
                color: app.mixColors(app.pageColor, app.inkColor, 0.09)

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: Math.round(6 * app.uiScale)
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: modelData.label
                    color: app.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(12 * app.uiScale)
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        overlay.visible = false;
                        app.activate(modelData.action);
                    }
                }
            }
        }
    }
}

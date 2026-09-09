import QtQuick

// The TABLE screen: the independent variable down the left, one column for
// every equation that is switched on.
Item {
    id: screen
    property var app
    readonly property int visibleRows: Math.max(4, Math.floor(height / Math.round(24 * app.uiScale)) - 1)

    Column {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: backend.revision, backend.tableRows(screen.visibleRows)

            Rectangle {
                width: screen.width
                height: Math.round(24 * app.uiScale)
                color: modelData.header ? app.mixColors(app.pageColor, app.inkColor, 0.1)
                                        : "transparent"

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: Math.round(4 * app.uiScale)

                    Text {
                        width: Math.round(90 * app.uiScale)
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.parameter
                        color: modelData.header ? app.accentColor : app.inkColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.round(13 * app.uiScale)
                    }

                    Repeater {
                        model: modelData.columns

                        Text {
                            width: Math.round(90 * app.uiScale)
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: app.inkColor
                            elide: Text.ElideRight
                            font.family: "iA Writer Mono S"
                            font.pixelSize: Math.round(13 * app.uiScale)
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onWheel: function (wheel) {
            backend.scrollTable(wheel.angleDelta.y > 0 ? -1 : 1);
        }
    }
}

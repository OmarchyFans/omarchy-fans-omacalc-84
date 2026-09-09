import QtQuick

// The Y= screen. Each row switches its equation on or off, picks a line
// style, and holds the expression itself.
Item {
    id: editor
    property var app

    Column {
        anchors.fill: parent
        spacing: Math.round(4 * app.uiScale)

        Text {
            text: ["Function  Y=", "Parametric", "Polar", "Sequence"][backend.graphMode]
            color: app.mutedColor
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.round(13 * app.uiScale)
        }

        Repeater {
            model: backend.functions

            Row {
                width: editor.width
                spacing: Math.round(6 * app.uiScale)

                Rectangle {
                    width: Math.round(30 * app.uiScale)
                    height: Math.round(30 * app.uiScale)
                    radius: width / 2
                    color: modelData.enabled ? app.accentColor : "transparent"
                    border.width: 1
                    border.color: app.mixColors(app.pageColor, app.inkColor, 0.3)

                    Text {
                        anchors.centerIn: parent
                        text: ["—", "▬", "⋯"][modelData.style]
                        color: modelData.enabled ? app.pageColor : app.mutedColor
                        font.pixelSize: Math.round(13 * app.uiScale)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: backend.setFunctionEnabled(modelData.index, !modelData.enabled)
                        onPressAndHold: backend.setFunctionStyle(modelData.index,
                                                                 (modelData.style + 1) % 3)
                    }
                }

                Text {
                    width: Math.round(38 * app.uiScale)
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name + "="
                    color: app.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(14 * app.uiScale)
                }

                FieldInput {
                    app: editor.app
                    width: parent.width - Math.round(80 * app.uiScale)
                    text: modelData.body
                    placeholder: "expression in " + app.graphParameter()
                    onAccepted: backend.setFunctionBody(modelData.index, text)
                    onActiveFocusChanged: if (!activeFocus) backend.setFunctionBody(modelData.index, text)
                }
            }
        }
    }
}

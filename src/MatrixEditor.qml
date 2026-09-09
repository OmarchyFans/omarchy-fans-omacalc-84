import QtQuick

// The matrix editor: pick a name, set the dimensions, fill in the cells.
Item {
    id: editor
    property var app
    property string current: "[A]"
    readonly property var contents: backend.revision, backend.matrixData(current)

    Column {
        anchors.fill: parent
        spacing: Math.round(6 * app.uiScale)

        Flow {
            width: parent.width
            spacing: Math.round(4 * app.uiScale)

            Repeater {
                model: backend.matrixNames

                Rectangle {
                    width: Math.round(34 * app.uiScale)
                    height: Math.round(26 * app.uiScale)
                    radius: Math.round(4 * app.uiScale)
                    color: modelData === editor.current
                           ? app.accentColor : app.mixColors(app.pageColor, app.inkColor, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: modelData === editor.current ? app.pageColor : app.inkColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.round(12 * app.uiScale)
                    }
                    MouseArea { anchors.fill: parent; onClicked: editor.current = modelData }
                }
            }
        }

        Row {
            spacing: Math.round(6 * app.uiScale)
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: editor.current + "  size"
                color: app.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * app.uiScale)
            }
            FieldInput {
                id: rowsField
                app: editor.app
                width: Math.round(50 * app.uiScale)
                text: String(editor.contents.rows)
                onAccepted: backend.resizeMatrix(editor.current, parseInt(text),
                                                 parseInt(columnsField.text))
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "×"
                color: app.mutedColor
                font.pixelSize: Math.round(13 * app.uiScale)
            }
            FieldInput {
                id: columnsField
                app: editor.app
                width: Math.round(50 * app.uiScale)
                text: String(editor.contents.columns)
                onAccepted: backend.resizeMatrix(editor.current, parseInt(rowsField.text),
                                                 parseInt(text))
            }
        }

        Flickable {
            width: parent.width
            height: parent.height - Math.round(80 * app.uiScale)
            contentWidth: grid.width
            contentHeight: grid.height
            clip: true

            Column {
                id: grid
                spacing: 2

                Repeater {
                    model: editor.contents.rows

                    Row {
                        id: matrixRow
                        property int rowIndex: index
                        spacing: 2

                        Repeater {
                            model: editor.contents.columns

                            FieldInput {
                                app: editor.app
                                width: Math.round(74 * app.uiScale)
                                implicitHeight: Math.round(26 * app.uiScale)
                                text: editor.contents.cells[matrixRow.rowIndex][index]
                                onAccepted: backend.setMatrixCell(editor.current,
                                                                  matrixRow.rowIndex, index, text)
                                onEditingFinished: backend.setMatrixCell(editor.current,
                                                                        matrixRow.rowIndex,
                                                                        index, text)
                            }
                        }
                    }
                }
            }
        }
    }
}

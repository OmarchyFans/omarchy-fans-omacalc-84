import QtQuick

// The list editor: L1 through L6 side by side, one editable cell per row.
Item {
    id: editor
    property var app
    readonly property int rowCount: {
        var longest = 0;
        var all = backend.lists;
        for (var i = 0; i < all.length; ++i)
            longest = Math.max(longest, all[i].values.length);
        return longest + 4;
    }

    Flickable {
        anchors.fill: parent
        contentWidth: row.width
        contentHeight: row.height
        clip: true

        Row {
            id: row
            spacing: Math.round(4 * app.uiScale)

            Column {
                spacing: 1
                Text {
                    text: " "
                    height: Math.round(22 * app.uiScale)
                    font.pixelSize: Math.round(12 * app.uiScale)
                }
                Repeater {
                    model: editor.rowCount
                    Text {
                        width: Math.round(28 * app.uiScale)
                        height: Math.round(26 * app.uiScale)
                        verticalAlignment: Text.AlignVCenter
                        text: String(index + 1)
                        color: app.mutedColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.round(11 * app.uiScale)
                    }
                }
            }

            Repeater {
                model: backend.lists

                Column {
                    id: listColumn
                    property var listData: modelData
                    spacing: 1

                    Text {
                        width: Math.round(88 * app.uiScale)
                        height: Math.round(22 * app.uiScale)
                        text: listColumn.listData.name
                        color: app.accentColor
                        font.family: "iA Writer Mono S"
                        font.bold: true
                        font.pixelSize: Math.round(13 * app.uiScale)
                    }

                    Repeater {
                        model: editor.rowCount

                        FieldInput {
                            app: editor.app
                            width: Math.round(88 * app.uiScale)
                            implicitHeight: Math.round(26 * app.uiScale)
                            text: index < listColumn.listData.values.length
                                  ? listColumn.listData.values[index] : ""
                            onAccepted: backend.setListCell(listColumn.listData.name, index, text)
                            onEditingFinished: backend.setListCell(listColumn.listData.name,
                                                                  index, text)
                        }
                    }
                }
            }
        }
    }
}

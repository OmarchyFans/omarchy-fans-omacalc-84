import QtQuick

// STAT TESTS. The backend describes each procedure's parameters, so this one
// screen builds the form for every test and interval.
Item {
    id: screen
    property var app
    property var procedure: null
    property var results: []
    property var answers: ({})

    function choose(entry) {
        screen.procedure = entry;
        var initial = {};
        for (var i = 0; i < entry.fields.length; ++i)
            initial[entry.fields[i].key] = entry.fields[i]["default"];
        screen.answers = initial;
        screen.results = [];
    }

    function run() {
        if (screen.procedure)
            screen.results = backend.runInference(screen.procedure.key, screen.answers);
    }

    function setAnswer(key, value) {
        // A fresh object, so the change actually reaches the bindings.
        var next = Object.assign({}, screen.answers);
        next[key] = value;
        screen.answers = next;
    }

    // The list of procedures, shown until one is picked.
    GridView {
        anchors.fill: parent
        visible: screen.procedure === null
        clip: true
        cellWidth: Math.round(width / 2)
        cellHeight: Math.round(30 * app.uiScale)
        model: backend.inferenceProcedures()

        delegate: Rectangle {
            width: GridView.view.cellWidth - Math.round(4 * app.uiScale)
            height: GridView.view.cellHeight - Math.round(4 * app.uiScale)
            radius: Math.round(4 * app.uiScale)
            color: app.mixColors(app.pageColor, app.inkColor, 0.09)

            Text {
                anchors.fill: parent
                anchors.leftMargin: Math.round(6 * app.uiScale)
                verticalAlignment: Text.AlignVCenter
                text: modelData.name
                color: app.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * app.uiScale)
            }
            MouseArea { anchors.fill: parent; onClicked: screen.choose(modelData) }
        }
    }

    Column {
        anchors.fill: parent
        visible: screen.procedure !== null
        spacing: Math.round(5 * app.uiScale)

        Row {
            width: parent.width
            spacing: Math.round(8 * app.uiScale)

            Text {
                text: "‹ tests"
                color: app.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * app.uiScale)
                MouseArea { anchors.fill: parent; onClicked: screen.procedure = null }
            }
            Text {
                text: screen.procedure ? screen.procedure.name : ""
                color: app.accentColor
                font.family: "iA Writer Mono S"
                font.bold: true
                font.pixelSize: Math.round(14 * app.uiScale)
            }
        }

        Repeater {
            model: screen.procedure ? screen.procedure.fields : []

            Row {
                id: fieldRow
                property var spec: modelData
                width: screen.width
                spacing: Math.round(6 * app.uiScale)

                Text {
                    width: Math.round(64 * app.uiScale)
                    anchors.verticalCenter: parent.verticalCenter
                    text: fieldRow.spec.label
                    color: app.mutedColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(12 * app.uiScale)
                }

                FieldInput {
                    visible: fieldRow.spec.type !== "choice"
                    app: screen.app
                    width: Math.round(110 * app.uiScale)
                    implicitHeight: Math.round(26 * app.uiScale)
                    text: String(fieldRow.spec["default"])
                    onTextChanged: screen.setAnswer(fieldRow.spec.key, text)
                    onAccepted: screen.run()
                }

                Row {
                    visible: fieldRow.spec.type === "choice"
                    spacing: Math.round(3 * app.uiScale)

                    Repeater {
                        model: fieldRow.spec.options ? fieldRow.spec.options : []

                        Rectangle {
                            width: Math.round(38 * app.uiScale)
                            height: Math.round(26 * app.uiScale)
                            radius: Math.round(4 * app.uiScale)
                            color: screen.answers[fieldRow.spec.key] === index
                                   ? app.accentColor
                                   : app.mixColors(app.pageColor, app.inkColor, 0.09)
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: screen.answers[fieldRow.spec.key] === index
                                       ? app.pageColor : app.inkColor
                                font.family: "iA Writer Mono S"
                                font.pixelSize: Math.round(12 * app.uiScale)
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: screen.setAnswer(fieldRow.spec.key, index)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            width: Math.round(80 * app.uiScale)
            height: Math.round(28 * app.uiScale)
            radius: Math.round(4 * app.uiScale)
            color: app.accentColor
            Text {
                anchors.centerIn: parent
                text: "Calculate"
                color: app.pageColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(11 * app.uiScale)
            }
            MouseArea { anchors.fill: parent; onClicked: screen.run() }
        }

        Repeater {
            model: screen.results

            Row {
                spacing: Math.round(10 * app.uiScale)
                Text {
                    width: Math.round(70 * app.uiScale)
                    text: modelData.label
                    color: app.mutedColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(13 * app.uiScale)
                }
                Text {
                    text: modelData.value
                    color: app.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(13 * app.uiScale)
                }
            }
        }
    }
}

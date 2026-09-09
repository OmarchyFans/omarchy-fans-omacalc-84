import QtQuick

// Where 1-Var Stats, 2-Var Stats and the regressions print their answers.
// The list names stay editable so the same screen can be run again.
Item {
    id: screen
    property var app
    property string title: ""
    property var rows: []
    property string xList: "L1"
    property string yList: "L2"
    property int model: -1     // regression model, or -1
    property int variables: 1  // 1 or 2

    function recalculate() {
        if (screen.model >= 0)
            screen.rows = backend.regression(screen.model, screen.xList, screen.yList, -1);
        else if (screen.variables === 2)
            screen.rows = backend.twoVariableStats(screen.xList, screen.yList);
        else
            screen.rows = backend.oneVariableStats(screen.xList, "");
    }

    function storeToY1() {
        if (screen.model >= 0)
            backend.regression(screen.model, screen.xList, screen.yList, 0);
    }

    Column {
        anchors.fill: parent
        spacing: Math.round(6 * app.uiScale)

        Text {
            text: screen.title
            color: app.accentColor
            font.family: "iA Writer Mono S"
            font.bold: true
            font.pixelSize: Math.round(15 * app.uiScale)
        }

        Row {
            width: parent.width
            spacing: Math.round(6 * app.uiScale)

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Xlist"
                color: app.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * app.uiScale)
            }
            FieldInput {
                app: screen.app
                width: Math.round(60 * app.uiScale)
                text: screen.xList
                onAccepted: { screen.xList = text; screen.recalculate(); }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: screen.variables === 2 || screen.model >= 0
                text: "Ylist"
                color: app.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * app.uiScale)
            }
            FieldInput {
                app: screen.app
                visible: screen.variables === 2 || screen.model >= 0
                width: Math.round(60 * app.uiScale)
                text: screen.yList
                onAccepted: { screen.yList = text; screen.recalculate(); }
            }
            Rectangle {
                visible: screen.model >= 0
                width: Math.round(78 * app.uiScale)
                height: Math.round(28 * app.uiScale)
                radius: Math.round(4 * app.uiScale)
                color: app.mixColors(app.pageColor, app.inkColor, 0.12)
                Text {
                    anchors.centerIn: parent
                    text: "store Y1"
                    color: app.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(11 * app.uiScale)
                }
                MouseArea { anchors.fill: parent; onClicked: screen.storeToY1() }
            }
        }

        ListView {
            width: parent.width
            height: parent.height - Math.round(80 * app.uiScale)
            clip: true
            model: screen.rows
            spacing: Math.round(2 * app.uiScale)

            delegate: Row {
                spacing: Math.round(10 * app.uiScale)
                Text {
                    width: Math.round(70 * app.uiScale)
                    text: modelData.label
                    color: app.mutedColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(14 * app.uiScale)
                }
                Text {
                    text: modelData.value
                    color: app.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(14 * app.uiScale)
                }
            }
        }
    }
}

import QtQuick

// The home screen: everything entered so far, with the line being typed at
// the bottom. Answers are right aligned under their expression, as they are
// on the calculator.
Item {
    id: home
    property var app
    property alias entryField: entry

    function submit() {
        var line = entry.text;
        if (line.trim() === "")
            return;
        var result = backend.submit(line);
        entry.text = "";
        historyView.positionViewAtEnd();
    }

    function recallLastEntry() {
        var items = backend.history;
        if (items.length > 0)
            entry.text = items[items.length - 1].expression;
    }

    Column {
        anchors.fill: parent
        spacing: Math.round(6 * app.uiScale)

        ListView {
            id: historyView
            width: parent.width
            height: parent.height - entry.height - Math.round(6 * app.uiScale)
            clip: true
            model: backend.history
            spacing: Math.round(6 * app.uiScale)
            onCountChanged: positionViewAtEnd()

            delegate: Column {
                width: historyView.width
                spacing: 1

                Text {
                    width: parent.width
                    text: modelData.expression
                    color: app.mutedColor
                    elide: Text.ElideLeft
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(15 * app.uiScale)
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    text: modelData.answer
                    color: modelData.ok ? app.inkColor : "#e06c75"
                    elide: Text.ElideLeft
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.round(19 * app.uiScale)
                }
            }
        }

        FieldInput {
            id: entry
            app: home.app
            width: parent.width
            placeholder: "enter an expression"
            onAccepted: home.submit()
        }
    }

    Component.onCompleted: entry.focusField()
}

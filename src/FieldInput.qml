import QtQuick

// A single editable field. Focusing one makes it the target for keypad
// presses, so the pad types into whichever box the cursor is in.
Rectangle {
    id: field

    property var app
    property alias text: input.text
    property alias cursorPosition: input.cursorPosition
    property string placeholder
    property bool numeric: false
    signal accepted()
    signal moveFocus(int direction)

    color: input.activeFocus ? app.mixColors(app.pageColor, app.inkColor, 0.12)
                             : app.mixColors(app.pageColor, app.inkColor, 0.06)
    border.width: 1
    border.color: input.activeFocus ? app.accentColor
                                    : app.mixColors(app.pageColor, app.inkColor, 0.14)
    radius: Math.round(4 * app.uiScale)
    implicitHeight: Math.round(30 * app.uiScale)

    function focusField() { input.forceActiveFocus(); }
    function insertText(value) { input.insert(input.cursorPosition, value); }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: Math.round(6 * app.uiScale)
        anchors.rightMargin: Math.round(6 * app.uiScale)
        verticalAlignment: TextInput.AlignVCenter
        clip: true
        color: app.inkColor
        selectionColor: app.accentColor
        selectedTextColor: app.pageColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.round(15 * app.uiScale)
        onActiveFocusChanged: if (activeFocus) app.activeInput = field
        onAccepted: field.accepted()
        Keys.onUpPressed: field.moveFocus(-1)
        Keys.onDownPressed: field.moveFocus(1)

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: input.text === "" && !input.activeFocus
            text: field.placeholder
            color: app.mutedColor
            font: input.font
        }
    }
}

import QtQuick

// One key. The small text above the cap is what the key does after 2nd or
// ALPHA, printed in the same blue and green as the legends on the case.
Rectangle {
    id: control

    property string label
    property string secondLabel
    property string alphaLabel
    property color baseColor: "#34353a"
    property color textColor: "#f4f4f2"
    property color secondColor: "#4b8fd6"
    property color alphaColor: "#63b463"
    property bool highlighted: false
    property bool wide: false
    property real uiScale: 1

    signal activated()
    signal held()

    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: activated()

    function lift(color, amount) {
        return Qt.rgba(color.r + (1 - color.r) * amount,
                       color.g + (1 - color.g) * amount,
                       color.b + (1 - color.b) * amount, 1);
    }

    radius: Math.max(3, Math.round(Math.min(height * 0.3, 9 * uiScale)))
    color: {
        var base = highlighted ? lift(baseColor, 0.25) : baseColor;
        if (hitArea.pressed)
            return Qt.darker(base, 1.25);
        if (hitArea.containsMouse)
            return lift(base, 0.09);
        return base;
    }

    // A thin lit top edge and a darker floor give the cap its moulded look.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: parent.radius * 0.5
        height: Math.max(1, Math.round(parent.height * 0.06))
        radius: height / 2
        color: control.lift(control.color, 0.16)
        visible: !hitArea.pressed
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: Math.round(2 * control.uiScale)
        spacing: Math.round(5 * control.uiScale)
        visible: control.secondLabel !== "" || control.alphaLabel !== ""

        Text {
            text: control.secondLabel
            color: control.secondColor
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.max(6, Math.round(control.height * 0.27))
        }
        Text {
            text: control.alphaLabel
            color: control.alphaColor
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.max(6, Math.round(control.height * 0.27))
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - Math.round(4 * control.uiScale)
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: control.label
        color: control.textColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.round(Math.min(control.height * 0.42,
                                            control.width / Math.max(2.0, control.label.length * 0.66)))
    }

    MouseArea {
        id: hitArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: control.activated()
        onPressAndHold: control.held()
    }
}

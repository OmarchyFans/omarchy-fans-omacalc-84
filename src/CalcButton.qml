import QtQuick

// One keypad key. The small text above the cap is what the key does after
// 2nd (left) or ALPHA (right), the way the labels are printed on the case.
Rectangle {
    id: control

    property string label
    property string secondLabel
    property string alphaLabel
    property string kind: "number"  // number, operator, function, menu, enter, modifier
    property bool highlighted: false
    property color pageColor: "#101010"
    property color inkColor: "#eeeeee"
    property color accentColor: "#5584aa"
    property real uiScale: 1

    signal activated()
    signal held()

    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: activated()

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }

    readonly property real restingLift: {
        if (kind === "operator") return 0.16;
        if (kind === "menu") return 0.11;
        if (kind === "function") return 0.09;
        return 0.05;
    }
    readonly property real activeLift: restingLift
        + (hitArea.pressed ? 0.09 : (hitArea.containsMouse ? 0.045 : 0))

    radius: Math.min(10 * uiScale, height * 0.22)
    color: {
        if (highlighted)
            return accentColor;
        if (kind === "enter")
            return mixColors(inkColor, pageColor, hitArea.pressed ? 0.22 : (hitArea.containsMouse ? 0.1 : 0));
        return mixColors(pageColor, inkColor, activeLift);
    }
    border.width: kind === "number" ? 1 : 0
    border.color: mixColors(pageColor, inkColor, 0.13)

    // The printed 2nd and ALPHA legends sit above the key itself.
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.round(2 * control.uiScale)
        spacing: Math.round(4 * control.uiScale)
        visible: control.secondLabel !== "" || control.alphaLabel !== ""

        Text {
            text: control.secondLabel
            color: control.accentColor
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.max(7, Math.round(control.height * 0.19))
        }
        Text {
            text: control.alphaLabel
            color: control.mixColors(control.pageColor, control.inkColor, 0.55)
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.max(7, Math.round(control.height * 0.19))
        }
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: (control.secondLabel !== "" || control.alphaLabel !== "")
            ? Math.round(control.height * 0.09) : 0
        width: parent.width - 6
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: control.label
        color: {
            if (control.highlighted) return control.pageColor;
            if (control.kind === "enter") return control.pageColor;
            return control.inkColor;
        }
        font.family: "iA Writer Mono S"
        font.bold: control.kind === "enter" || control.kind === "modifier"
        font.pixelSize: Math.round(Math.min(control.height * 0.36,
                                            control.width / Math.max(2.2, control.label.length * 0.62)))
    }

    MouseArea {
        id: hitArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: control.activated()
        onPressAndHold: control.held()
    }
}

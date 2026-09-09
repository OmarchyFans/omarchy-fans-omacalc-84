import QtQuick
import Omacalc

// The graph itself, with the trace readout along the bottom and the prompt
// bar the CALC menu uses to ask for its bounds.
Item {
    id: screen
    property var app
    property var prompt: null
    property alias view: view

    function askFor(kind, fields) {
        var values = [];
        var window = backend.window;
        for (var i = 0; i < fields.length; ++i)
            values.push(fields[i].value);
        screen.prompt = { kind: kind, labels: fields.map(function (f) { return f.label; }),
                          values: values };
    }

    function runPrompt() {
        if (!screen.prompt)
            return;
        var numbers = [];
        for (var i = 0; i < promptRepeater.count; ++i)
            numbers.push(parseFloat(promptRepeater.itemAt(i).text));
        var index = backend.traceFunction;
        var result = null;
        switch (screen.prompt.kind) {
        case "value": result = backend.calcValue(index, numbers[0]); break;
        case "zero": result = backend.calcZero(index, numbers[0], numbers[1]); break;
        case "minimum": result = backend.calcExtremum(index, numbers[0], numbers[1], false); break;
        case "maximum": result = backend.calcExtremum(index, numbers[0], numbers[1], true); break;
        case "intersect": result = backend.calcIntersect(index, app.secondEnabledFunction(),
                                                         numbers[0], numbers[1]); break;
        case "derivative": result = backend.calcDerivative(index, numbers[0]); break;
        case "integral": result = backend.calcIntegral(index, numbers[0], numbers[1]); break;
        }
        screen.prompt = null;
        if (result)
            readout.text = result.ok
                ? result.label + "   X=" + result.xText + "   " +
                  (result.label === "∫f(x)dx" || result.label === "dy/dx" ? "= " : "Y=") + result.yText
                : result.label + ": not found in that range";
    }

    Column {
        anchors.fill: parent
        spacing: Math.round(4 * app.uiScale)

        GraphView {
            id: view
            width: parent.width
            height: parent.height - readout.height - promptRow.height
                    - Math.round(8 * app.uiScale)
            backend: app.calculator
            pageColor: app.pageColor
            inkColor: app.inkColor
            accentColor: app.accentColor
            uiScale: app.uiScale

            MouseArea {
                anchors.fill: parent
                onClicked: function (mouse) {
                    // Tapping the graph moves the trace cursor there.
                    backend.traceTo(view.toGraphX(mouse.x));
                }
                onWheel: function (wheel) {
                    if (wheel.angleDelta.y > 0)
                        backend.zoomIn();
                    else
                        backend.zoomOut();
                }
            }
        }

        Row {
            id: promptRow
            width: parent.width
            spacing: Math.round(6 * app.uiScale)
            visible: screen.prompt !== null
            height: visible ? Math.round(32 * app.uiScale) : 0

            Repeater {
                id: promptRepeater
                model: screen.prompt ? screen.prompt.labels.length : 0

                Row {
                    spacing: Math.round(4 * app.uiScale)
                    property alias text: promptField.text

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: screen.prompt.labels[index] + "="
                        color: app.inkColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.round(13 * app.uiScale)
                    }
                    FieldInput {
                        id: promptField
                        app: screen.app
                        width: Math.round(90 * app.uiScale)
                        text: String(screen.prompt.values[index])
                        onAccepted: screen.runPrompt()
                    }
                }
            }
        }

        Text {
            id: readout
            width: parent.width
            text: backend.tracing ? backend.traceLabel : ""
            color: app.inkColor
            elide: Text.ElideRight
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.round(13 * app.uiScale)
        }
    }
}

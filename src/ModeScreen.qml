import QtQuick

// The MODE screen. Each row is one setting with its choices spelled out.
Item {
    id: screen
    property var app

    function choose(setting, value) {
        switch (setting) {
        case "angle": backend.angleMode = value; break;
        case "format": backend.numberFormat = value; break;
        case "fix": backend.fixDigits = value === 0 ? -1 : value - 1; break;
        case "complex": backend.complexMode = value; break;
        case "graph": backend.graphMode = value; break;
        case "draw": backend.connectedPlot = value === 0; break;
        case "grid": backend.showGrid = value === 1; break;
        case "axes": backend.showAxes = value === 0; break;
        }
    }

    readonly property var settings: [
        { setting: "angle", label: "Angle", options: ["Radian", "Degree"], current: backend.angleMode },
        { setting: "format", label: "Notation", options: ["Normal", "Sci", "Eng"], current: backend.numberFormat },
        { setting: "fix", label: "Decimals",
          options: ["Float", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"],
          current: backend.fixDigits < 0 ? 0 : backend.fixDigits + 1 },
        { setting: "complex", label: "Complex", options: ["Real", "a+bi", "re^θi"], current: backend.complexMode },
        { setting: "graph", label: "Graphing", options: ["Func", "Par", "Pol", "Seq"], current: backend.graphMode },
        { setting: "draw", label: "Plot", options: ["Connected", "Dot"], current: backend.connectedPlot ? 0 : 1 },
        { setting: "grid", label: "Grid", options: ["Off", "On"], current: backend.showGrid ? 1 : 0 },
        { setting: "axes", label: "Axes", options: ["On", "Off"], current: backend.showAxes ? 0 : 1 }
    ]

    Flickable {
        anchors.fill: parent
        contentHeight: column.height
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: Math.round(8 * app.uiScale)

            Repeater {
                model: screen.settings

                Column {
                    id: settingRow
                    property var entry: modelData
                    width: column.width
                    spacing: Math.round(3 * app.uiScale)

                    Text {
                        text: settingRow.entry.label
                        color: app.mutedColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.round(12 * app.uiScale)
                    }

                    Flow {
                        width: parent.width
                        spacing: Math.round(4 * app.uiScale)

                        Repeater {
                            model: settingRow.entry.options

                            Rectangle {
                                width: Math.max(Math.round(40 * app.uiScale),
                                                optionText.implicitWidth + Math.round(14 * app.uiScale))
                                height: Math.round(28 * app.uiScale)
                                radius: Math.round(4 * app.uiScale)
                                color: index === settingRow.entry.current
                                       ? app.accentColor
                                       : app.mixColors(app.pageColor, app.inkColor, 0.08)
                                Text {
                                    id: optionText
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: index === settingRow.entry.current ? app.pageColor : app.inkColor
                                    font.family: "iA Writer Mono S"
                                    font.pixelSize: Math.round(12 * app.uiScale)
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: screen.choose(settingRow.entry.setting, index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

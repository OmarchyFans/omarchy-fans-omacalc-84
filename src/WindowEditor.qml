import QtQuick

// WINDOW and TBLSET in one list: every variable that decides what the graph
// and the table show.
Item {
    id: editor
    property var app

    readonly property var commonKeys: [
        { key: "xMin", label: "Xmin" }, { key: "xMax", label: "Xmax" },
        { key: "xScale", label: "Xscl" }, { key: "yMin", label: "Ymin" },
        { key: "yMax", label: "Ymax" }, { key: "yScale", label: "Yscl" },
        { key: "xResolution", label: "Xres" }
    ]
    readonly property var modeKeys: {
        if (backend.graphMode === 1)
            return [{ key: "tMin", label: "Tmin" }, { key: "tMax", label: "Tmax" },
                    { key: "tStep", label: "Tstep" }];
        if (backend.graphMode === 2)
            return [{ key: "thetaMin", label: "θmin" }, { key: "thetaMax", label: "θmax" },
                    { key: "thetaStep", label: "θstep" }];
        if (backend.graphMode === 3)
            return [{ key: "nMin", label: "nMin" }, { key: "nMax", label: "nMax" }];
        return [];
    }
    readonly property var tableKeys: [
        { key: "tableStart", label: "TblStart" }, { key: "tableStep", label: "ΔTbl" }
    ]

    Flickable {
        anchors.fill: parent
        contentHeight: column.height
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: Math.round(4 * app.uiScale)

            Repeater {
                model: editor.commonKeys.concat(editor.modeKeys).concat(editor.tableKeys)

                Row {
                    width: column.width
                    spacing: Math.round(8 * app.uiScale)

                    Text {
                        width: Math.round(72 * app.uiScale)
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: app.inkColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.round(14 * app.uiScale)
                    }

                    FieldInput {
                        app: editor.app
                        width: parent.width - Math.round(80 * app.uiScale)
                        text: app.formatWindowValue(backend.window[modelData.key])
                        onAccepted: backend.setWindowValue(modelData.key, text)
                        onActiveFocusChanged: if (!activeFocus) backend.setWindowValue(modelData.key, text)
                    }
                }
            }
        }
    }
}

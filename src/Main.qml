import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import Omacalc

ApplicationWindow {
    id: win
    width: 470
    height: 810
    minimumWidth: 360
    minimumHeight: 560
    visible: true
    title: "Omacalc"

    // Named so a child can point a GraphView at it: writing `backend: backend`
    // inside the item would resolve to the item's own property instead.
    readonly property var calculator: backend
    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: backend.themeBackground
    readonly property color inkColor: backend.themeForeground
    readonly property color accentColor: backend.themeAccent
    // Every size in the interface is expressed at the 470 × 810 design size;
    // resizing the window scales the whole face with it.
    readonly property real uiScale: Math.min(width / 470, height / 810)
    property real appliedTextScale: backend.textScale

    property int screen: initialScreen
    property bool secondActive: false
    property bool alphaActive: false
    property bool alphaLock: false
    property var activeInput: null

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }
    readonly property color mutedColor: mixColors(pageColor, inkColor, 0.5)

    function graphParameter() {
        return ["X", "T", "θ", "n"][backend.graphMode];
    }

    function formatWindowValue(value) {
        if (value === undefined || value === null)
            return "";
        return String(Math.round(value * 1e10) / 1e10);
    }

    function secondEnabledFunction() {
        var items = backend.functions;
        for (var i = 0; i < items.length; ++i) {
            if (items[i].enabled && items[i].body !== "" && i !== backend.traceFunction)
                return i;
        }
        return backend.traceFunction;
    }

    function insert(text) {
        if (activeInput) {
            activeInput.insertText(text);
            activeInput.focusField();
        }
    }

    function showScreen(index) {
        screen = index;
        if (index === 0)
            homeScreen.entryField.focusField();
    }

    // --- Menu contents -----------------------------------------------------

    function entry(label, action) { return { label: label, action: action }; }

    readonly property var mathMenu: [
        entry("▶Frac", "i:▶Frac"), entry("▶Dec", "i:▶Dec"), entry("³", "i:³"),
        entry("∛(", "i:∛("), entry("ˣ√", "i:ˣ√"), entry("fMin(", "i:fMin("),
        entry("fMax(", "i:fMax("), entry("nDeriv(", "i:nDeriv("), entry("fnInt(", "i:fnInt("),
        entry("solve(", "i:solve("), entry("abs(", "i:abs("), entry("round(", "i:round("),
        entry("iPart(", "i:iPart("), entry("fPart(", "i:fPart("), entry("int(", "i:int("),
        entry("min(", "i:min("), entry("max(", "i:max("), entry("lcm(", "i:lcm("),
        entry("gcd(", "i:gcd("), entry("remainder(", "i:remainder("), entry("rand", "i:rand"),
        entry("nPr", "i: nPr "), entry("nCr", "i: nCr "), entry("!", "i:!"),
        entry("randInt(", "i:randInt("), entry("randNorm(", "i:randNorm("),
        entry("randBin(", "i:randBin("), entry("conj(", "i:conj("), entry("real(", "i:real("),
        entry("imag(", "i:imag("), entry("angle(", "i:angle("), entry("▶Rect", "i:▶Rect"),
        entry("▶Polar", "i:▶Polar"), entry("logBASE(", "i:logBASE(")
    ]

    readonly property var testMenu: [
        entry("=", "i:="), entry("≠", "i:≠"), entry(">", "i:>"), entry("≥", "i:≥"),
        entry("<", "i:<"), entry("≤", "i:≤"), entry("and", "i: and "), entry("or", "i: or "),
        entry("xor", "i: xor "), entry("not(", "i:not(")
    ]

    readonly property var angleMenu: [
        entry("°", "i:°"), entry("ʳ", "i:ʳ"), entry("▶DMS", "i:▶DMS")
    ]

    readonly property var matrixMenu: [
        entry("[A]", "i:[A]"), entry("[B]", "i:[B]"), entry("[C]", "i:[C]"), entry("[D]", "i:[D]"),
        entry("[E]", "i:[E]"), entry("[F]", "i:[F]"), entry("[G]", "i:[G]"), entry("[H]", "i:[H]"),
        entry("[I]", "i:[I]"), entry("[J]", "i:[J]"), entry("det(", "i:det("),
        entry("ᵀ", "i:ᵀ"), entry("dim(", "i:dim("), entry("identity(", "i:identity("),
        entry("ref(", "i:ref("), entry("rref(", "i:rref("), entry("augment(", "i:augment("),
        entry("randM(", "i:randM("), entry("edit matrices", "s:7")
    ]

    readonly property var listMenu: [
        entry("L1", "i:L1"), entry("L2", "i:L2"), entry("L3", "i:L3"), entry("L4", "i:L4"),
        entry("L5", "i:L5"), entry("L6", "i:L6"), entry("sum(", "i:sum("),
        entry("prod(", "i:prod("), entry("mean(", "i:mean("), entry("median(", "i:median("),
        entry("stdDev(", "i:stdDev("), entry("variance(", "i:variance("),
        entry("cumSum(", "i:cumSum("), entry("ΔList(", "i:ΔList("), entry("sortA(", "i:sortA("),
        entry("sortD(", "i:sortD("), entry("seq(", "i:seq("), entry("augment(", "i:augment("),
        entry("dim(", "i:dim("), entry("fill(", "i:fill("), entry("edit lists", "s:5")
    ]

    readonly property var distrMenu: [
        entry("normalpdf(", "i:normalpdf("), entry("normalcdf(", "i:normalcdf("),
        entry("invNorm(", "i:invNorm("), entry("tpdf(", "i:tpdf("), entry("tcdf(", "i:tcdf("),
        entry("invT(", "i:invT("), entry("χ²pdf(", "i:χ²pdf("), entry("χ²cdf(", "i:χ²cdf("),
        entry("Fpdf(", "i:Fpdf("), entry("Fcdf(", "i:Fcdf("), entry("binompdf(", "i:binompdf("),
        entry("binomcdf(", "i:binomcdf("), entry("poissonpdf(", "i:poissonpdf("),
        entry("poissoncdf(", "i:poissoncdf("), entry("geometpdf(", "i:geometpdf("),
        entry("geometcdf(", "i:geometcdf(")
    ]

    readonly property var zoomMenu: [
        entry("ZStandard", "z:standard"), entry("ZTrig", "z:trig"), entry("ZDecimal", "z:decimal"),
        entry("ZSquare", "z:square"), entry("Zoom In", "z:in"), entry("Zoom Out", "z:out"),
        entry("ZoomFit", "z:fit"), entry("ZInteger", "z:integer"), entry("ZoomStat", "z:stat"),
        entry("ZPrevious", "z:previous")
    ]

    readonly property var calcMenu: [
        entry("value", "k:value"), entry("zero", "k:zero"), entry("minimum", "k:minimum"),
        entry("maximum", "k:maximum"), entry("intersect", "k:intersect"),
        entry("dy/dx", "k:derivative"), entry("∫f(x)dx", "k:integral")
    ]

    readonly property var statMenu: [
        entry("edit lists", "s:5"), entry("1-Var Stats", "t:1"), entry("2-Var Stats", "t:2"),
        entry("LinReg(ax+b)", "r:0"), entry("LinReg(a+bx)", "r:1"), entry("QuadReg", "r:2"),
        entry("CubicReg", "r:3"), entry("QuartReg", "r:4"), entry("LnReg", "r:5"),
        entry("ExpReg", "r:6"), entry("PwrReg", "r:7"), entry("Logistic", "r:8"),
        entry("SinReg", "r:9"), entry("Med-Med", "r:10"), entry("TESTS", "s:9")
    ]

    readonly property var varsMenu: [
        entry("Xmin", "i:Xmin"), entry("Xmax", "i:Xmax"), entry("Ymin", "i:Ymin"),
        entry("Ymax", "i:Ymax"), entry("Y1", "i:Y1"), entry("Y2", "i:Y2"), entry("Y3", "i:Y3"),
        entry("Y4", "i:Y4"), entry("Y5", "i:Y5"), entry("Y6", "i:Y6"),
        entry("π", "i:π"), entry("e", "i:e"), entry("Ans", "i:Ans"), entry("θ", "i:θ")
    ]

    readonly property var formatMenu: [
        entry(backend.showGrid ? "GridOff" : "GridOn", "c:togglegrid"),
        entry(backend.showAxes ? "AxesOff" : "AxesOn", "c:toggleaxes"),
        entry(backend.connectedPlot ? "Dot" : "Connected", "c:toggleconnected")
    ]

    readonly property var statPlotMenu: [
        entry("Plot1 scatter L1,L2", "p:0,0"), entry("Plot1 xyLine L1,L2", "p:0,1"),
        entry("Plot1 histogram L1", "p:0,2"), entry("Plot1 box plot L1", "p:0,3"),
        entry("Plot1 normal prob L1", "p:0,4"), entry("Plot1 off", "p:0,-1"),
        entry("Plot2 scatter L3,L4", "p:1,0"), entry("Plot2 off", "p:1,-1"),
        entry("Plot3 scatter L5,L6", "p:2,0"), entry("Plot3 off", "p:2,-1")
    ]

    readonly property var catalogMenu: {
        var names = ["abs(", "acosh(", "and", "angle(", "asinh(", "atanh(", "augment(",
            "binomcdf(", "binompdf(", "cbrt(", "conj(", "cos(", "cos⁻¹(", "cosh(", "cumSum(",
            "det(", "dim(", "e^(", "exp(", "fill(", "fMax(", "fMin(", "fnInt(", "Fcdf(", "Fpdf(",
            "gcd(", "geometcdf(", "geometpdf(", "identity(", "imag(", "int(", "invNorm(", "invT(",
            "iPart(", "fPart(", "lcm(", "ln(", "log(", "logBASE(", "max(", "mean(", "median(",
            "min(", "nCr", "nDeriv(", "normalcdf(", "normalpdf(", "not(", "nPr", "or", "poissoncdf(",
            "poissonpdf(", "prod(", "rand", "randBin(", "randInt(", "randM(", "randNorm(", "real(",
            "ref(", "remainder(", "round(", "rref(", "seq(", "sign(", "sin(", "sin⁻¹(", "sinh(",
            "solve(", "sortA(", "sortD(", "stdDev(", "sum(", "tan(", "tan⁻¹(", "tanh(", "tcdf(",
            "tpdf(", "variance(", "xor", "ΔList(", "χ²cdf(", "χ²pdf("];
        var items = [];
        for (var i = 0; i < names.length; ++i)
            items.push(entry(names[i], "i:" + names[i]));
        return items;
    }

    // --- Actions -----------------------------------------------------------

    function activate(spec) {
        if (!spec)
            return;
        var kind = spec.substring(0, 1);
        var rest = spec.substring(2);

        if (kind === "i") {
            insert(rest);
        } else if (kind === "s") {
            showScreen(parseInt(rest));
        } else if (kind === "m") {
            openMenu(rest);
        } else if (kind === "z") {
            runZoom(rest);
        } else if (kind === "k") {
            runCalc(rest);
        } else if (kind === "t") {
            results.model = -1;
            results.variables = parseInt(rest);
            results.title = rest === "2" ? "2-Var Stats" : "1-Var Stats";
            results.recalculate();
            showScreen(6);
        } else if (kind === "r") {
            results.model = parseInt(rest);
            results.variables = 2;
            results.title = statMenu.filter(function (item) { return item.action === spec; })[0].label;
            results.recalculate();
            showScreen(6);
        } else if (kind === "p") {
            var parts = rest.split(",");
            var index = parseInt(parts[0]);
            var type = parseInt(parts[1]);
            var xLists = ["L1", "L3", "L5"];
            var yLists = ["L2", "L4", "L6"];
            backend.setPlot(index, type >= 0, Math.max(0, type), xLists[index], yLists[index], 0);
            if (type >= 0)
                showScreen(3);
        } else if (kind === "c") {
            runCommand(rest);
        }
    }

    function openMenu(name) {
        var menus = {
            "math": ["MATH", mathMenu], "test": ["TEST", testMenu], "angle": ["ANGLE", angleMenu],
            "matrix": ["MATRIX", matrixMenu], "list": ["LIST", listMenu],
            "distr": ["DISTR", distrMenu], "zoom": ["ZOOM", zoomMenu], "calc": ["CALC", calcMenu],
            "stat": ["STAT", statMenu], "vars": ["VARS", varsMenu], "format": ["FORMAT", formatMenu],
            "statplot": ["STAT PLOT", statPlotMenu], "catalog": ["CATALOG", catalogMenu]
        };
        var chosen = menus[name];
        if (chosen)
            menuOverlay.open(chosen[0], chosen[1]);
    }

    function runZoom(name) {
        switch (name) {
        case "standard": backend.zoomStandard(); break;
        case "trig": backend.zoomTrig(); break;
        case "decimal": backend.zoomDecimal(); break;
        case "square": backend.zoomSquare(graphScreen.view.width / Math.max(1, graphScreen.view.height)); break;
        case "in": backend.zoomIn(); break;
        case "out": backend.zoomOut(); break;
        case "fit": backend.zoomFit(); break;
        case "integer": backend.zoomInteger(); break;
        case "stat": backend.zoomStatistics(); break;
        case "previous": backend.zoomPrevious(); break;
        }
        showScreen(3);
    }

    function runCalc(name) {
        showScreen(3);
        var w = backend.window;
        var left = formatWindowValue(w.xMin);
        var right = formatWindowValue(w.xMax);
        var middle = formatWindowValue((w.xMin + w.xMax) / 2);
        if (name === "value" || name === "derivative")
            graphScreen.askFor(name, [{ label: "X", value: middle }]);
        else if (name === "integral")
            graphScreen.askFor(name, [{ label: "Lower", value: left }, { label: "Upper", value: right }]);
        else
            graphScreen.askFor(name, [{ label: "Left", value: left }, { label: "Right", value: right }]);
    }

    function runCommand(name) {
        switch (name) {
        case "second":
            secondActive = !secondActive;
            alphaActive = false;
            break;
        case "alpha":
            alphaActive = !alphaActive;
            alphaLock = false;
            secondActive = false;
            break;
        case "alphalock":
            alphaLock = !alphaLock;
            alphaActive = alphaLock;
            break;
        case "enter":
            if (screen === 3 && graphScreen.prompt)
                graphScreen.runPrompt();
            else if (screen === 0)
                homeScreen.submit();
            else if (activeInput)
                activeInput.accepted();
            break;
        case "entry":
            showScreen(0);
            homeScreen.recallLastEntry();
            break;
        case "clear":
            if (screen === 0) {
                if (activeInput && activeInput.text !== "")
                    activeInput.text = "";
                else
                    backend.clearHistory();
            } else if (activeInput) {
                activeInput.text = "";
            }
            break;
        case "del":
            if (activeInput) {
                var position = activeInput.cursorPosition;
                if (position > 0) {
                    activeInput.text = activeInput.text.substring(0, position - 1)
                        + activeInput.text.substring(position);
                    activeInput.cursorPosition = position - 1;
                }
                activeInput.focusField();
            }
            break;
        case "left":
            if (screen === 3) backend.traceStep(-1, graphScreen.view.width);
            else if (activeInput) activeInput.cursorPosition = Math.max(0, activeInput.cursorPosition - 1);
            break;
        case "right":
            if (screen === 3) backend.traceStep(1, graphScreen.view.width);
            else if (activeInput) activeInput.cursorPosition = activeInput.cursorPosition + 1;
            break;
        case "up":
            if (screen === 3) backend.traceSelect(-1);
            else if (screen === 4) backend.scrollTable(-1);
            break;
        case "down":
            if (screen === 3) backend.traceSelect(1);
            else if (screen === 4) backend.scrollTable(1);
            break;
        case "trace":
            showScreen(3);
            backend.startTrace();
            break;
        case "quit":
            showScreen(0);
            backend.stopTrace();
            break;
        case "home":
            showScreen(0);
            break;
        case "xton":
            insert(graphParameter());
            break;
        case "rcl":
            insert("Ans");
            break;
        case "ins":
            break;
        case "togglegrid": backend.showGrid = !backend.showGrid; showScreen(3); break;
        case "toggleaxes": backend.showAxes = !backend.showAxes; showScreen(3); break;
        case "toggleconnected": backend.connectedPlot = !backend.connectedPlot; showScreen(3); break;
        }
    }

    function pressKey(key) {
        if (secondActive && key.second) {
            secondActive = false;
            activate(key.second);
            return;
        }
        if (alphaActive && key.alpha) {
            if (!alphaLock)
                alphaActive = false;
            insert(key.alpha);
            return;
        }
        secondActive = false;
        activate(key.action);
    }

    // --- Key map -----------------------------------------------------------

    function key(label, action, second, alpha, secondLabel, kind) {
        return { label: label, action: action, second: second || "", alpha: alpha || "",
                 secondLabel: secondLabel || "", kind: kind || "number" };
    }

    readonly property var keypad: [
        key("Y=", "s:1", "m:statplot", "", "PLOT", "menu"),
        key("WIND", "s:2", "s:2", "", "TBLSET", "menu"),
        key("ZOOM", "m:zoom", "m:format", "", "FORMAT", "menu"),
        key("TRACE", "c:trace", "m:calc", "", "CALC", "menu"),
        key("GRAPH", "s:3", "s:4", "", "TABLE", "menu"),

        key("2nd", "c:second", "", "", "", "modifier"),
        key("MODE", "s:8", "c:quit", "", "QUIT", "menu"),
        key("DEL", "c:del", "c:ins", "", "INS", "function"),
        key("◀", "c:left", "", "", "", "function"),
        key("▶", "c:right", "", "", "", "function"),

        key("ALPHA", "c:alpha", "c:alphalock", "", "LOCK", "modifier"),
        key("X,T,θ,n", "c:xton", "", "", "", "function"),
        key("STAT", "m:stat", "m:list", "", "LIST", "menu"),
        key("▲", "c:up", "", "", "", "function"),
        key("▼", "c:down", "", "", "", "function"),

        key("MATH", "m:math", "m:test", "A", "TEST", "menu"),
        key("MATRX", "m:matrix", "m:angle", "B", "ANGLE", "menu"),
        key("VARS", "m:vars", "m:distr", "C", "DISTR", "menu"),
        key("CLEAR", "c:clear", "", "D", "", "function"),
        key("CATLG", "m:catalog", "", "E", "", "menu"),

        key("x⁻¹", "i:⁻¹", "i:[A]", "F", "[A]", "function"),
        key("sin", "i:sin(", "i:sin⁻¹(", "G", "sin⁻¹", "function"),
        key("cos", "i:cos(", "i:cos⁻¹(", "H", "cos⁻¹", "function"),
        key("tan", "i:tan(", "i:tan⁻¹(", "I", "tan⁻¹", "function"),
        key("^", "i:^", "i:π", "J", "π", "operator"),

        key("x²", "i:²", "i:√(", "K", "√", "function"),
        key(",", "i:,", "i:E", "L", "EE", "function"),
        key("(", "i:(", "i:{", "M", "{", "function"),
        key(")", "i:)", "i:}", "N", "}", "function"),
        key("÷", "i:/", "i:e", "O", "e", "operator"),

        key("log", "i:log(", "i:10^(", "P", "10ˣ", "function"),
        key("7", "i:7", "", "Q", "", "number"),
        key("8", "i:8", "", "R", "", "number"),
        key("9", "i:9", "", "S", "", "number"),
        key("×", "i:*", "i:[", "T", "[", "operator"),

        key("ln", "i:ln(", "i:e^(", "U", "eˣ", "function"),
        key("4", "i:4", "i:L4", "V", "L4", "number"),
        key("5", "i:5", "i:L5", "W", "L5", "number"),
        key("6", "i:6", "i:L6", "X", "L6", "number"),
        key("−", "i:-", "i:]", "Y", "]", "operator"),

        key("STO▶", "i:→", "c:rcl", "Z", "RCL", "function"),
        key("1", "i:1", "i:L1", "θ", "L1", "number"),
        key("2", "i:2", "i:L2", " ", "L2", "number"),
        key("3", "i:3", "i:L3", "\"", "L3", "number"),
        key("+", "i:+", "i:θ", ":", "θ", "operator"),

        key("HOME", "c:home", "", "", "", "function"),
        key("0", "i:0", "i:∛(", "_", "∛", "number"),
        key(".", "i:.", "i:i", "?", "i", "number"),
        key("(−)", "i:-", "i:Ans", "!", "ANS", "number"),
        key("ENTER", "c:enter", "c:entry", "", "ENTRY", "enter")
    ]

    Connections {
        target: backend

        function onTextScaleChanged() {
            var factor = backend.textScale / win.appliedTextScale;
            win.appliedTextScale = backend.textScale;
            if (win.visibility === Window.Windowed) {
                win.width = Math.round(win.width * factor);
                win.height = Math.round(win.height * factor);
            }
        }
    }

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: accentColor
    color: pageColor

    Shortcut {
        sequences: ["Ctrl+C", "Meta+C"]
        context: Qt.ApplicationShortcut
        onActivated: backend.copyResult()
    }
    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: menuOverlay.visible ? menuOverlay.visible = false : win.showScreen(0)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Math.round(12 * win.uiScale)
        spacing: Math.round(8 * win.uiScale)

        // Status line: which screen, and what the modes are set to.
        RowLayout {
            Layout.fillWidth: true
            spacing: Math.round(8 * win.uiScale)

            Text {
                text: ["HOME", "Y=", "WINDOW", "GRAPH", "TABLE", "LISTS", "STATS", "MATRIX",
                       "MODE", "TESTS"][win.screen]
                color: win.accentColor
                font.family: "iA Writer Mono S"
                font.bold: true
                font.pixelSize: Math.round(12 * win.uiScale)
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (win.secondActive ? "2nd " : "") + (win.alphaActive ? "A " : "")
                      + ["RAD", "DEG"][backend.angleMode] + " "
                      + ["FUNC", "PAR", "POL", "SEQ"][backend.graphMode]
                      + (backend.complexMode > 0 ? " a+bi" : "")
                color: win.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.round(12 * win.uiScale)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(parent.height * 0.42)
            color: win.mixColors(win.pageColor, win.inkColor, 0.04)
            border.width: 1
            border.color: win.mixColors(win.pageColor, win.inkColor, 0.14)
            radius: Math.round(6 * win.uiScale)
            clip: true

            StackLayout {
                anchors.fill: parent
                anchors.margins: Math.round(8 * win.uiScale)
                currentIndex: win.screen

                HomeScreen { id: homeScreen; app: win }
                EquationEditor { app: win }
                WindowEditor { app: win }
                GraphScreen { id: graphScreen; app: win }
                TableScreen { app: win }
                ListEditor { app: win }
                ResultsScreen { id: results; app: win }
                MatrixEditor { app: win }
                ModeScreen { app: win }
                TestsScreen { app: win }
            }

            MenuOverlay { id: menuOverlay; app: win }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 5
            rowSpacing: Math.round(5 * win.uiScale)
            columnSpacing: Math.round(5 * win.uiScale)

            Repeater {
                model: win.keypad

                CalcButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: modelData.label
                    secondLabel: modelData.secondLabel
                    alphaLabel: modelData.alpha
                    kind: modelData.kind
                    highlighted: (modelData.label === "2nd" && win.secondActive)
                                 || (modelData.label === "ALPHA" && win.alphaActive)
                    pageColor: win.pageColor
                    inkColor: win.inkColor
                    accentColor: win.accentColor
                    uiScale: win.uiScale
                    onActivated: win.pressKey(modelData)
                }
            }
        }
    }

    // Remember the last windowed geometry rather than whatever the window
    // happens to measure at teardown.
    property rect normalGeometry: Qt.rect(x, y, width, height)
    property bool wasMaximized: false

    function trackNormalGeometry() {
        if (visibility === Window.Windowed)
            normalGeometry = Qt.rect(x, y, width, height);
    }

    onXChanged: trackNormalGeometry()
    onYChanged: trackNormalGeometry()
    onWidthChanged: trackNormalGeometry()
    onHeightChanged: trackNormalGeometry()

    onVisibilityChanged: function () {
        if (win.visibility === Window.Maximized || win.visibility === Window.FullScreen)
            wasMaximized = true;
        else if (win.visibility === Window.Windowed)
            wasMaximized = false;
    }

    Component.onCompleted: {
        var geometry = backend.windowGeometry();
        if (geometry.valid) {
            x = geometry.x;
            y = geometry.y;
            width = geometry.width;
            height = geometry.height;
            if (geometry.maximized) showMaximized();
        } else {
            width = Math.round(470 * backend.textScale);
            height = Math.round(810 * backend.textScale);
        }
    }

    Component.onDestruction: backend.saveWindowGeometry(
        normalGeometry.x, normalGeometry.y,
        normalGeometry.width, normalGeometry.height, wasMaximized)
}

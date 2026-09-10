import QtQuick
import Quickshell
import qs.Commons
import qs.Ui

// Bar button that opens the calculator. The binary is built from source by
// install.sh, so this looks for it on PATH first and then in the plugin's own
// build directory, and says what to do when it finds neither.
BarWidget {
  id: root
  moduleName: "fans.omarchy.omagraph"

  readonly property string localBinary: Qt.resolvedUrl("build/omagraph").toString().replace(/^file:\/\//, "")

  readonly property bool opened: false
  function open() { root.launch() }
  function close() {}

  function launch(screen) {
    var command = "if command -v omagraph >/dev/null 2>&1; then exec omagraph ${1:+--screen \"$1\"};"
      + " elif [ -x \"$0\" ]; then exec \"$0\" ${1:+--screen \"$1\"};"
      + " else notify-send 'Omagraph' 'Run install.sh in the plugin folder to build the calculator.'; fi"
    Quickshell.execDetached(["sh", "-c", command, root.localBinary, screen || ""])
  }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "󰃬"                    // nf-md-calculator
    slotSize: Style.bar.statusSlot
    fontSize: Style.font.caption
    tooltipText: "Omagraph (right click: graph screen)"
    onPressed: function(b) {
      if (b === Qt.RightButton) root.launch("graph")
      else root.launch()
    }
  }
}

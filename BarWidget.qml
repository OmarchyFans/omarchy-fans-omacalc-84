import QtQuick
import Quickshell
import qs.Commons
import qs.Ui

// Bar button that opens the calculator. Launching is done by Launcher.qml,
// which only ever runs this plugin's own build/omacalc-84 by absolute path,
// after checking its ownership and permissions, with a closed environment.
BarWidget {
  id: root
  moduleName: "fans.omarchy.omacalc-84"

  readonly property bool opened: false
  function open() { launcher.launch() }
  function close() {}

  Launcher { id: launcher }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "󰃬"                    // nf-md-calculator
    slotSize: Style.bar.statusSlot
    fontSize: Style.font.caption
    tooltipText: "OmaCalc-84 (right click: graph screen)"
    onPressed: function(b) {
      if (b === Qt.RightButton) launcher.launch("graph")
      else launcher.launch()
    }
  }
}

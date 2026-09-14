import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

// Bar button that opens the calculator. Launching is done by Launcher.qml,
// which only ever runs this plugin's own files by absolute path, after checking
// their ownership and permissions, with a closed environment.
//
// Updates (docs/update-alerts.md): lib/update.sh, run through the same
// Launcher checks, looks up the published version on load and every six hours
// (cached, one small request to this plugin's repository). When a newer one is
// out, or the calculator was not rebuilt after a plugin update, a dot appears
// on the button and the next left click opens a small popup with what changed
// and an Update… button; Later hides that version and the click goes back to
// opening the calculator. Right click always opens the graph screen.
BarWidget {
  id: root
  moduleName: "fans.omarchy.omacalc-84"

  readonly property bool opened: false
  function open() { launcher.launch() }
  function close() {}

  Launcher { id: launcher }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  function openCalculator(screen) {
    if (updatePopup.open) updatePopup.open = false
    launcher.launch(screen)
  }

  // ---- updates ----------------------------------------------------------------
  readonly property string updater: launcher.pluginRoot + "/lib/update.sh"
  property string version: ""
  property var updateInfo: null
  property bool updateHidden: false
  readonly property bool updateAvailable: !!updateInfo && updateInfo.update_available === true
                                          && updateInfo.dismissed !== updateInfo.latest
  readonly property bool updateMismatch: !!updateInfo && updateInfo.mismatch === true
  readonly property bool updatePending: !updateHidden && (updateAvailable || updateMismatch)

  FileView {
    path: launcher.pluginRoot + "/manifest.json"
    printErrors: false
    onLoaded: {
      try { root.version = String(JSON.parse(text()).version || "") } catch (e) { root.version = "" }
      root.checkUpdates()
    }
  }
  function checkUpdates() {
    if (root.setting("update_check", true) === false || updateProc.running) return
    launcher.runScript(root.updater, ["check", root.version], updateProc)
  }
  Process {
    id: updateProc
    stdout: StdioCollector { id: updateOut; waitForEnd: true }
    onExited: function(code) { try { root.updateInfo = JSON.parse(String(updateOut.text || "")) } catch (e) { root.updateInfo = null } }
  }
  Timer { interval: 6 * 3600 * 1000; running: true; repeat: true; onTriggered: root.checkUpdates() }
  function runUpdate() {
    root.updateHidden = true
    updatePopup.open = false
    launcher.runScript(root.updater, ["run", root.updateAvailable ? "all" : "install"], null)
  }
  function dismissUpdate() {
    root.updateHidden = true
    updatePopup.open = false
    if (root.updateAvailable && root.updateInfo.latest) launcher.runScript(root.updater, ["dismiss", String(root.updateInfo.latest)], null)
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "󰃬"                    // nf-md-calculator
    slotSize: Style.bar.statusSlot
    fontSize: Style.font.caption
    tooltipText: "OmaCalc-84 (right click: graph screen)"
      + (root.updateAvailable ? " · " + root.updateInfo.latest + " is available" : (root.updateMismatch ? " · finish updating" : ""))
    onPressed: function(b) {
      if (b === Qt.RightButton) root.openCalculator("graph")
      else if (root.updatePending) updatePopup.open = !updatePopup.open
      else root.openCalculator()
    }
    Rectangle {
      visible: root.updatePending
      anchors.top: parent.top; anchors.right: parent.right
      anchors.margins: Style.space(3)
      width: Style.space(6); height: width; radius: width / 2
      color: Color.accent
    }
  }

  // ---- update popup (docs/update-alerts.md) -----------------------------------
  PopupCard {
    id: updatePopup
    anchorItem: button
    bar: root.bar
    contentWidth: fittedContentWidth(Style.space(380))
    contentHeight: fittedContentHeight(updateCol.implicitHeight)
    Column {
      id: updateCol
      width: parent.width
      spacing: Style.space(4)
      Text {
        width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
        text: root.updateAvailable
              ? "OmaCalc-84 " + root.updateInfo.latest + " is available (you have " + root.version + ")"
              : "Finish updating OmaCalc-84: the plugin is " + root.version + ", the calculator was built from an older version"
        color: Color.popups.text; font.family: Style.font.family; font.pixelSize: Style.font.body; font.bold: true
      }
      Repeater {
        model: root.updateAvailable ? root.updateInfo.notes.slice(0, 4) : []
        delegate: Text {
          required property var modelData
          width: updateCol.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
          text: "•  " + modelData
          color: Color.popups.text; opacity: 0.8; font.family: Style.font.family; font.pixelSize: Style.font.caption
        }
      }
      Text {
        width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
        text: root.updateAvailable
              ? "Update opens a terminal: omarchy plugin update shows the changes and asks, then install.sh asks before rebuilding the calculator."
              : "Run install.sh once to rebuild the calculator from the updated sources. It asks first."
        color: Color.popups.text; opacity: 0.6; font.family: Style.font.family; font.pixelSize: Style.font.caption
      }
      Item { width: 1; height: Style.space(2) }
      Row {
        spacing: Style.space(4)
        Button { text: root.updateAvailable ? "Update…" : "Finish update…"; bordered: true; foreground: Color.accent; onClicked: root.runUpdate() }
        Button { text: "Later"; bordered: true; foreground: Color.popups.text; onClicked: root.dismissUpdate() }
        Button { text: "Open calculator"; foreground: Color.popups.text; onClicked: root.openCalculator() }
      }
    }
  }
}

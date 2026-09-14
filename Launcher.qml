import QtQuick
import Quickshell
import Quickshell.Io

// Starts the calculator without trusting PATH. The only programs it will run
// are files inside this plugin: the binary install.sh builds (build/omacalc-84)
// and the update helper (lib/update.sh, through /usr/bin/bash), and only after
// /usr/bin/stat confirms that every directory leading to the file and the file
// itself belong to root or the current user, are not group or world writable,
// are not symlinks, and that the file is a regular executable file. Everything
// is started by absolute path with direct arguments and a closed environment.
Item {
  id: root

  // Absolute path of the plugin directory, without a trailing slash.
  property string pluginRoot: Qt.resolvedUrl(".").toString().replace(/^file:\/\//, "").replace(/\/$/, "")
  readonly property string binary: pluginRoot + "/build/omacalc-84"

  property string statTool: "/usr/bin/stat"
  property string notifyTool: "/usr/bin/notify-send"
  property string shellTool: "/usr/bin/bash"

  // Emitted after each attempt, for tests: ok is true when the program was started.
  signal finished(bool ok, string reason)

  // What the pending check will start once the paths pass: the file, its
  // arguments, whether it is a script for shellTool, and an optional
  // Quickshell Process to run it in (so its output can be read) instead of
  // starting it detached.
  property string pendingTarget: ""
  property var pendingArguments: []
  property bool pendingScript: false
  property var pendingProcess: null
  property bool pendingQuiet: false     // a refusal only emits finished(), no notification

  readonly property var passedVariables: [
    "HOME", "USER", "LOGNAME", "LANG", "LC_ALL", "XDG_RUNTIME_DIR", "WAYLAND_DISPLAY",
    "DISPLAY", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME",
    "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS",
    "XCURSOR_THEME", "XCURSOR_SIZE", "QT_QPA_PLATFORMTHEME"
  ]

  function environment() {
    var env = { "PATH": "/usr/bin", "QT_QPA_PLATFORM": "wayland" }
    for (var i = 0; i < passedVariables.length; i++) {
      var value = Quickshell.env(passedVariables[i])
      if (value !== null && value !== undefined && String(value) !== "")
        env[passedVariables[i]] = String(value)
    }
    return env
  }

  // Every directory from / down to the file's folder, then the file itself.
  function checkedPaths(target) {
    var paths = ["/"]
    var parts = target.split("/").filter(function(p) { return p.length > 0 })
    var current = ""
    for (var i = 0; i < parts.length; i++) {
      current += "/" + parts[i]
      paths.push(current)
    }
    return paths
  }

  function launch(screen) {
    start(binary, screen ? ["--screen", String(screen)] : [], false, null)
  }

  // Run one of this plugin's scripts after the same checks, either in PROCESS
  // (a Quickshell Process, whose command is set here) or detached. QUIET runs
  // (the automatic update check) never raise a notification when refused.
  function runScript(path, args, process, quiet) {
    start(path, args, true, process || null, quiet === true)
  }

  function start(target, args, script, process, quiet) {
    if (checker.running) return
    pendingTarget = target
    pendingArguments = args || []
    pendingScript = script
    pendingProcess = process
    pendingQuiet = quiet === true
    output.expected = checkedPaths(target)
    checker.command = [statTool, "-c", "%F|%u|%a", "/proc/self/stat"].concat(output.expected)
    checker.running = true
  }

  function refuse(reason) {
    if (!pendingQuiet) Quickshell.execDetached({
      command: [notifyTool, "OmaCalc-84", reason],
      clearEnvironment: true,
      environment: environment()
    })
    finished(false, reason)
  }

  // Returns an empty string when the stat output is acceptable, otherwise why not.
  function verify(text, exitCode) {
    var what = pendingScript ? "the update helper" : "the calculator"
    var missing = pendingScript
      ? "The plugin file " + pendingTarget + " is missing. Reinstall the plugin."
      : "The calculator is not built yet. Run install.sh in " + pluginRoot + "."
    if (exitCode !== 0) return missing
    var lines = text.split("\n").filter(function(l) { return l.length > 0 })
    var paths = output.expected
    if (lines.length !== paths.length + 1) return missing

    // /proc/self/stat is owned by whoever runs stat, which is this shell's user.
    var uid = lines[0].split("|")[1]
    if (!/^[0-9]+$/.test(uid)) return "Could not determine the current user."

    for (var i = 0; i < paths.length; i++) {
      var fields = lines[i + 1].split("|")
      var type = fields[0], owner = fields[1], mode = parseInt(fields[2], 8)
      var last = i === paths.length - 1
      var where = paths[i]
      if (isNaN(mode)) return "Unexpected stat output for " + where + "."
      if (last) {
        if (type !== "regular file") return where + " is not a regular file; refusing to run it."
        if (owner !== uid) return where + " is not owned by you; refusing to run it."
        if ((mode & 0o100) === 0) return where + " is not executable. Run install.sh again."
        if ((mode & 0o6000) !== 0) return where + " is setuid or setgid; refusing to run it."
      } else {
        if (type !== "directory") return where + " is not a plain directory; refusing to run " + what + "."
        if (owner !== uid && owner !== "0") return where + " is owned by another user; refusing to run " + what + "."
      }
      if ((mode & 0o022) !== 0) return where + " is group or world writable; refusing to run " + what + "."
    }
    return ""
  }

  StdioCollector {
    id: output
    property var expected: []
  }

  Process {
    id: checker
    stdout: output
    onExited: function(exitCode) {
      var reason = root.verify(output.text, exitCode)
      if (reason !== "") {
        root.refuse(reason)
        return
      }
      var argv = (root.pendingScript ? [root.shellTool, root.pendingTarget] : [root.pendingTarget]).concat(root.pendingArguments)
      if (root.pendingProcess) {
        root.pendingProcess.workingDirectory = root.pluginRoot
        root.pendingProcess.clearEnvironment = true
        root.pendingProcess.environment = root.environment()
        root.pendingProcess.command = argv
        root.pendingProcess.running = true
      } else {
        Quickshell.execDetached({
          command: argv,
          workingDirectory: root.pluginRoot,
          clearEnvironment: true,
          environment: root.environment()
        })
      }
      root.finished(true, "")
    }
  }
}

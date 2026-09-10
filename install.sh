#!/bin/bash
#
# Builds OmaCalc-84 and offers the four extras a plugin cannot ship on its own.
# `omarchy plugin add` installs the bar widget; everything below is optional,
# asked for one at a time, and idempotent:
#
#   1. build build/omacalc-84 from source (needs a Qt 6 qmake)
#   2. link it into ~/.local/bin
#   3. add a desktop entry so it shows up in the launcher
#   4. add a SUPER + ALT + C keybinding, and optionally point Omarchy's
#      calculator key (SUPER + CTRL + Q) at it instead of the built-in one
#
# Nothing is overwritten: existing entries are detected and skipped, and a
# timestamped backup is taken before any config file is appended to.
set -euo pipefail

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
MARK="fans.omarchy.omacalc-84"
BIN="$REPO/build/omacalc-84"
YES=0; [[ ${1:-} == --yes ]] && YES=1
ask() { (( YES )) && return 0; read -rp "$1 [y/N] " a; [[ $a == [yY]* ]]; }

# 1. Build
if [[ -x $BIN ]] && ! ask "Rebuild the calculator?"; then
  echo "  using the existing $BIN"
else
  if ! command -v qmake6 >/dev/null 2>&1 && ! command -v qmake >/dev/null 2>&1; then
    echo "OmaCalc-84 needs Qt 6 to build:" >&2
    echo "  sudo pacman -S --needed qt6-base qt6-declarative base-devel" >&2
    exit 1
  fi
  "$REPO/bin/build"
fi

# 2. On PATH
if ask "Link omacalc-84 into ~/.local/bin?"; then
  mkdir -p "$HOME/.local/bin"
  ln -sfn "$BIN" "$HOME/.local/bin/omacalc-84"
  echo "  linked ~/.local/bin/omacalc-84"
fi

# 3. Desktop entry, so it appears alongside the other applications
if ask "Add a desktop entry and icon?"; then
  mkdir -p "$HOME/.local/share/applications" \
           "$HOME/.local/share/icons/hicolor/scalable/apps"
  install -m 644 "$REPO/share/omacalc-84.svg" \
          "$HOME/.local/share/icons/hicolor/scalable/apps/omacalc-84.svg"
  sed "s|@BIN@|$BIN|" "$REPO/share/omacalc-84.desktop.in" \
      > "$HOME/.local/share/applications/omacalc-84.desktop"
  update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
  echo "  installed the desktop entry"
fi

# 4. Keybindings
B="$HOME/.config/hypr/bindings.lua"
BIND="o.bind(\"SUPER + ALT + C\", \"OmaCalc-84\", \"$BIN\")"
if [[ -f $B ]] && grep -qF "$BIND" "$B"; then
  echo "  keybinding already present in $B"
elif ask "Add keybinding SUPER + ALT + C -> OmaCalc-84 to $B?"; then
  [[ -f $B ]] && cp -a "$B" "$B.bak.$(date +%s)"
  cat >>"$B" <<LUA

-- OmaCalc-84 ($MARK). SUPER + ALT + C was unbound by default.
$BIND
LUA
  echo "  appended; run 'hyprctl reload && hyprctl configerrors' to verify"
fi

TAKEOVER="o.bind(\"SUPER + CTRL + Q\", \"Calculator\", \"$BIN\")"
if [[ -f $B ]] && grep -qF "$TAKEOVER" "$B"; then
  echo "  calculator key already points at OmaCalc-84"
elif ask "Point Omarchy's calculator key (SUPER + CTRL + Q) at OmaCalc-84 instead of omacalc?"; then
  [[ -f $B ]] && cp -a "$B" "$B.bak.$(date +%s)"
  cat >>"$B" <<LUA

-- OmaCalc-84 ($MARK) takes over the calculator key. Delete these two lines to
-- give it back to the calculator that ships with Omarchy.
$TAKEOVER
LUA
  echo "  appended; run 'hyprctl reload' to apply"
fi

echo
echo "Done. Put the button on the bar with: omarchy plugin enable $MARK right"

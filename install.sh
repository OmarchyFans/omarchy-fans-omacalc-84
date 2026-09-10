#!/bin/bash
#
# Builds Omagraph and offers the four extras a plugin cannot ship on its own.
# `omarchy plugin add` installs the bar widget; everything below is optional,
# asked for one at a time, and idempotent:
#
#   1. build build/omagraph from source (needs a Qt 6 qmake)
#   2. link it into ~/.local/bin
#   3. add a desktop entry so it shows up in the launcher
#   4. add a SUPER + ALT + C keybinding, and optionally point Omarchy's
#      calculator key (SUPER + CTRL + Q) at it instead of the built-in one
#
# Nothing is overwritten: existing entries are detected and skipped, and a
# timestamped backup is taken before any config file is appended to.
set -euo pipefail

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
MARK="fans.omarchy.omagraph"
BIN="$REPO/build/omagraph"
YES=0; [[ ${1:-} == --yes ]] && YES=1
ask() { (( YES )) && return 0; read -rp "$1 [y/N] " a; [[ $a == [yY]* ]]; }

# 1. Build
if [[ -x $BIN ]] && ! ask "Rebuild the calculator?"; then
  echo "  using the existing $BIN"
else
  if ! command -v qmake6 >/dev/null 2>&1 && ! command -v qmake >/dev/null 2>&1; then
    echo "Omagraph needs Qt 6 to build:" >&2
    echo "  sudo pacman -S --needed qt6-base qt6-declarative base-devel" >&2
    exit 1
  fi
  "$REPO/bin/build"
fi

# 2. On PATH
if ask "Link omagraph into ~/.local/bin?"; then
  mkdir -p "$HOME/.local/bin"
  ln -sfn "$BIN" "$HOME/.local/bin/omagraph"
  echo "  linked ~/.local/bin/omagraph"
fi

# 3. Desktop entry, so it appears alongside the other applications
if ask "Add a desktop entry and icon?"; then
  mkdir -p "$HOME/.local/share/applications" \
           "$HOME/.local/share/icons/hicolor/scalable/apps"
  install -m 644 "$REPO/share/omagraph.svg" \
          "$HOME/.local/share/icons/hicolor/scalable/apps/omagraph.svg"
  sed "s|@BIN@|$BIN|" "$REPO/share/omagraph.desktop.in" \
      > "$HOME/.local/share/applications/omagraph.desktop"
  update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
  echo "  installed the desktop entry"
fi

# 4. Keybindings
B="$HOME/.config/hypr/bindings.lua"
BIND="o.bind(\"SUPER + ALT + C\", \"Omagraph\", \"$BIN\")"
if [[ -f $B ]] && grep -qF "$BIND" "$B"; then
  echo "  keybinding already present in $B"
elif ask "Add keybinding SUPER + ALT + C -> Omagraph to $B?"; then
  [[ -f $B ]] && cp -a "$B" "$B.bak.$(date +%s)"
  cat >>"$B" <<LUA

-- Omagraph ($MARK). SUPER + ALT + C was unbound by default.
$BIND
LUA
  echo "  appended; run 'hyprctl reload && hyprctl configerrors' to verify"
fi

TAKEOVER="o.bind(\"SUPER + CTRL + Q\", \"Calculator\", \"$BIN\")"
if [[ -f $B ]] && grep -qF "$TAKEOVER" "$B"; then
  echo "  calculator key already points at Omagraph"
elif ask "Point Omarchy's calculator key (SUPER + CTRL + Q) at Omagraph instead of omacalc?"; then
  [[ -f $B ]] && cp -a "$B" "$B.bak.$(date +%s)"
  cat >>"$B" <<LUA

-- Omagraph ($MARK) takes over the calculator key. Delete these two lines to
-- give it back to the calculator that ships with Omarchy.
$TAKEOVER
LUA
  echo "  appended; run 'hyprctl reload' to apply"
fi

echo
echo "Done. Enable the bar button with: omarchy-shell bar add $MARK"

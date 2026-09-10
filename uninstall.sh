#!/bin/bash
# Reverses install.sh: the link, the desktop entry and both keybindings. The
# plugin itself is removed with `omarchy plugin remove fans.omarchy.omagraph`,
# and your saved window, equations and lists live in
# ~/.config/omarchy.fans/omagraph.conf, which is left alone.
set -euo pipefail
MARK="fans.omarchy.omagraph"

L="$HOME/.local/bin/omagraph"
[[ -L $L || -f $L ]] && { rm -f "$L"; echo "  removed $L"; }

for f in "$HOME/.local/share/applications/omagraph.desktop" \
         "$HOME/.local/share/icons/hicolor/scalable/apps/omagraph.svg"; do
  [[ -f $f ]] && { rm -f "$f"; echo "  removed $f"; }
done

B="$HOME/.config/hypr/bindings.lua"
if [[ -f $B ]] && grep -q "$MARK" "$B"; then
  cp -a "$B" "$B.bak.$(date +%s)"
  # Each block is a comment followed by its binding; the takeover block has a
  # two-line comment.
  sed -i "/-- Omagraph ($MARK)\. SUPER + ALT + C/,+1d" "$B"
  sed -i "/-- Omagraph ($MARK) takes over the calculator key/,+2d" "$B"
  echo "  keybindings removed from $B (run 'hyprctl reload')"
fi

echo "Done."

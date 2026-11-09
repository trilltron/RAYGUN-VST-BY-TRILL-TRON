#!/bin/bash
# RAYGUN by TRILL TRON - macOS installer (per-user, no admin password needed)
cd "$(dirname "$0")" || exit 1
echo "Installing RAYGUN..."
VST3="$HOME/Library/Audio/Plug-Ins/VST3"; AU="$HOME/Library/Audio/Plug-Ins/Components"; APPS="$HOME/Applications"
mkdir -p "$VST3" "$AU" "$APPS"
install_bundle() {  # $1 = bundle in this folder, $2 = destination folder
  [ -e "$1" ] || return 0
  rm -rf "$2/$1"; cp -R "$1" "$2/"
  xattr -cr "$2/$1" 2>/dev/null                     # remove the "downloaded from the internet" quarantine flag
  codesign --force --deep -s - "$2/$1" 2>/dev/null  # ad-hoc signature so Apple Silicon will load it
  echo "  installed $2/$1"
}
install_bundle RAYGUN.vst3 "$VST3"
install_bundle RAYGUN.component "$AU"
install_bundle RAYGUN.app "$APPS"
killall -9 AudioComponentRegistrar 2>/dev/null     # make Logic/GarageBand re-scan the Audio Unit
echo
echo "DONE."
echo "  VST3  -> $VST3/RAYGUN.vst3   (Ableton, FL Studio, Reaper, Bitwig, Cubase, Studio One)"
echo "  AU    -> $AU/RAYGUN.component (Logic Pro, GarageBand, MainStage)"
echo "  App   -> $APPS/RAYGUN.app    (standalone)"
echo "Restart your DAW and rescan plugins. See README_macOS.txt."
read -n 1 -s -r -p "Press any key to close..."

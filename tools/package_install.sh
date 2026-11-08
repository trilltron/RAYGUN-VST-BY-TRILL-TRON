#!/usr/bin/env bash
# Assemble the ready-to-ship install folder + zip from a finished Windows build.
# usage: tools/package_install.sh <path-to-RAYGUN.vst3-bundle> [path-to-RAYGUN.exe] [outdir]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VST3="$1"; EXE="${2:-}"; OUT="${3:-$ROOT/install/RAYGUN_Install_Windows}"; VST2="${4:-}"
rm -rf "$OUT"; mkdir -p "$OUT"
cp -r "$VST3" "$OUT/RAYGUN.vst3"
[ -n "$EXE" ] && [ -f "$EXE" ] && cp "$EXE" "$OUT/RAYGUN.exe"
[ -n "$VST2" ] && [ -f "$VST2" ] && cp "$VST2" "$OUT/RAYGUN_VST2.dll"
cp "$ROOT"/install/Windows/*.bat "$ROOT"/install/Windows/README_FL_STUDIO.txt "$OUT/"
mkdir -p "$OUT/Presets_Reference" && cp "$ROOT"/presets/factory/*.rgp "$OUT/Presets_Reference/"   # same presets as embedded, for browsing/sharing
mkdir -p "$OUT/Licenses" && cp "$ROOT"/data/fonts/OFL-*.txt "$OUT/Licenses/"
if command -v zip >/dev/null 2>&1; then ( cd "$(dirname "$OUT")" && rm -f "$(basename "$OUT").zip" && zip -qr "$(basename "$OUT").zip" "$(basename "$OUT")" ); fi
echo "packaged: $OUT  (+ .zip)"

#!/usr/bin/env bash
# usage: tools/package_mac.sh <build-artefacts-dir> <out-dir>   (artefacts-dir contains VST3/, AU/, Standalone/)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; ART="$1"; OUT="$2"
rm -rf "$OUT"; mkdir -p "$OUT"
cp -R "$ART/VST3/RAYGUN.vst3" "$OUT/"
[ -d "$ART/AU/RAYGUN.component" ] && cp -R "$ART/AU/RAYGUN.component" "$OUT/"
[ -d "$ART/Standalone/RAYGUN.app" ] && cp -R "$ART/Standalone/RAYGUN.app" "$OUT/"
for b in "$OUT"/*.vst3 "$OUT"/*.component "$OUT"/*.app; do [ -e "$b" ] && codesign --force --deep -s - "$b" || true; done   # ad-hoc sign
cp "$ROOT"/install/macOS/* "$OUT/"; chmod +x "$OUT"/*.command
mkdir -p "$OUT/Licenses" && cp "$ROOT"/data/fonts/OFL-*.txt "$OUT/Licenses/"
( cd "$(dirname "$OUT")" && rm -f RAYGUN_macOS.zip && zip -qry RAYGUN_macOS.zip "$(basename "$OUT")" )
echo "packaged: $OUT + RAYGUN_macOS.zip"

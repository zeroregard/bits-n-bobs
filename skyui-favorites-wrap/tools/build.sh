#!/bin/sh
# Build the wrap-around favoritesmenu.swf from the SkyUI installed in your game.
#   SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh [outdir]
# Only ScrollingList is re-imported; every other class keeps SkyUI's original bytecode.
set -eu
T=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-"$T/../dist/build"}
: "${SKYUI_BSA:?set SKYUI_BSA to your SkyUI_SE.bsa}"
: "${FFDEC:?set FFDEC, e.g. 'java -jar /opt/ffdec/ffdec.jar'}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
python3 "$T/bsax.py" "$SKYUI_BSA" --extract "$W/bsa" >/dev/null
FM="$W/bsa/interface/favoritesmenu.swf"
$FFDEC -export script "$W/fm" "$FM" >/dev/null 2>&1
python3 "$T/patch_wrap.py" "$W/fm/scripts"
mkdir -p "$W/only/scripts/__Packages/skyui/components/list" "$OUT/Interface"
cp "$W/fm/scripts/__Packages/skyui/components/list/ScrollingList.as" "$W/only/scripts/__Packages/skyui/components/list/"
$FFDEC -importScript "$FM" "$OUT/Interface/favoritesmenu.swf" "$W/only" >/dev/null 2>&1
ls -l "$OUT/Interface/favoritesmenu.swf"

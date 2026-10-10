#!/bin/sh
# Build the patched quest_journal.swf from the SkyUI installed in your game.
#   SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh [outdir]
# Only QuestsPage is re-imported; every other class keeps SkyUI's original bytecode.
set -eu
T=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-"$T/../dist/build"}
: "${SKYUI_BSA:?set SKYUI_BSA to your SkyUI_SE.bsa}"
: "${FFDEC:?set FFDEC, e.g. 'java -jar /opt/ffdec/ffdec.jar'}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
python3 "$T/bsax.py" "$SKYUI_BSA" --extract "$W/bsa" >/dev/null
QJ="$W/bsa/interface/quest_journal.swf"
$FFDEC -export script "$W/qj" "$QJ" >/dev/null 2>&1
python3 "$T/patch_qj.py" "$W/qj/scripts"
mkdir -p "$W/only/scripts/__Packages" "$OUT/Interface"
cp "$W/qj/scripts/__Packages/QuestsPage.as" "$W/only/scripts/__Packages/"
$FFDEC -importScript "$QJ" "$OUT/Interface/quest_journal.swf" "$W/only" >/dev/null 2>&1
ls -l "$OUT/Interface/quest_journal.swf"

#!/bin/sh
# Build the patched SWFs from the SkyUI installed in your game.
#   SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh [outdir]
# Only the patched classes are re-imported; every other class keeps SkyUI's original
# bytecode (a full JPEXS round-trip altered 64/71 classes and broke list scrolling).
set -eu
T=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-"$T/../dist/build"}
: "${SKYUI_BSA:?set SKYUI_BSA to your SkyUI_SE.bsa}"
: "${FFDEC:?set FFDEC, e.g. 'java -jar /opt/ffdec/ffdec.jar'}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
python3 "$T/bsax.py" "$SKYUI_BSA" --extract "$W/bsa" >/dev/null
MM="$W/bsa/interface/magicmenu.swf"; IL="$W/bsa/interface/skyui/inventorylists.swf"
$FFDEC -export script "$W/mm" "$MM" >/dev/null 2>&1
$FFDEC -export script "$W/il" "$IL" >/dev/null 2>&1
python3 "$T/patch_aec.py" "$W/mm/scripts"
python3 "$T/patch_aec_lists.py" "$W/il/scripts"
mkdir -p "$W/mm_only/scripts/__Packages" "$W/il_only/scripts/__Packages"
cp "$W/mm/scripts/__Packages/MagicDataSetter.as" "$W/mm/scripts/__Packages/MagicMenu.as" \
   "$W/mm/scripts/__Packages/MagicIconSetter.as" "$W/mm_only/scripts/__Packages/"
cp "$W/il/scripts/__Packages/InventoryLists.as" "$W/il_only/scripts/__Packages/"
mkdir -p "$OUT/Interface/SkyUI"
$FFDEC -importScript "$MM" "$OUT/Interface/magicmenu.swf" "$W/mm_only" >/dev/null 2>&1
$FFDEC -importScript "$IL" "$OUT/Interface/SkyUI/inventorylists.swf" "$W/il_only" >/dev/null 2>&1
# tab icons: three new labelled frames in a copy of the category icon SWF
ICO="$W/bsa/interface/skyui/icons_category_psychosteve.swf"; SVG="$T/../icons"
python3 "$T/patch_icons.py" frames "$ICO" "$W/ico1.swf"
$FFDEC -replace "$W/ico1.swf" "$W/ico2.swf" 900 "$SVG/effects_ongoing.svg" nofill \
  902 "$SVG/effects_harmful.svg" nofill 904 "$SVG/effects_boons.svg" nofill >/dev/null 2>&1
python3 "$T/patch_icons.py" place "$W/ico2.swf" "$OUT/Interface/SkyUI/icons_category_psychosteve.swf"
ls -l "$OUT/Interface/magicmenu.swf" "$OUT/Interface/SkyUI/inventorylists.swf" "$OUT/Interface/SkyUI/icons_category_psychosteve.swf"

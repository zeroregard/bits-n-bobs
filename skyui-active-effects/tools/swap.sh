#!/usr/bin/env bash
# Swap the deployed magicmenu.swf between baseline / feature / off.
#
#   ./swap.sh baseline   unmodified round-trip build  (proves the toolchain)
#   ./swap.sh feature    the actual mod
#   ./swap.sh off        remove the loose file, fall back to SkyUI's BSA copy
#   ./swap.sh status     what is deployed right now
#
# A loose Data/Interface/magicmenu.swf overrides SkyUI's BSA, so "off" is a
# clean revert - nothing in the BSA is ever touched.

set -u
DATA="${SKYRIM_DATA:-/f/Games/Steam/steamapps/common/Skyrim Special Edition/Data}"
DIST="$(cd "$(dirname "${BASH_SOURCE[0]}")/../dist" && pwd)"
TARGET="$DATA/Interface/magicmenu.swf"

md5of() { [ -f "$1" ] && md5sum "$1" 2>/dev/null | cut -c1-12 || echo "-"; }

case "${1:-status}" in
  baseline|feature)
    src="$DIST/magicmenu.swf"
    [ "$1" = baseline ] && src="$DIST/magicmenu.baseline.swf"
    [ -f "$src" ] || { echo "missing $src"; exit 1; }
    mkdir -p "$DATA/Interface"
    rm -f "$TARGET"            # delete first: never write through a Vortex hardlink
    cp "$src" "$TARGET"
    echo "deployed $1 ($(stat -c %s "$TARGET") bytes)"
    ;;
  off)
    rm -f "$TARGET" && echo "removed loose override; SkyUI's BSA copy is active again"
    ;;
  status)
    if [ ! -f "$TARGET" ]; then
      echo "no loose override - SkyUI's own magicmenu.swf is in use"
    else
      h=$(md5of "$TARGET")
      case "$h" in
        "$(md5of "$DIST/magicmenu.swf")")          echo "deployed: FEATURE  ($h)" ;;
        "$(md5of "$DIST/magicmenu.baseline.swf")") echo "deployed: BASELINE ($h)" ;;
        *)                                          echo "deployed: unknown build ($h)" ;;
      esac
    fi
    ;;
  *) sed -n '2,12p' "${BASH_SOURCE[0]}"; exit 1 ;;
esac

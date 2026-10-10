# SkyUI Favorites Wrap

In SkyUI's favorites menu, pressing Up on the first entry jumps to the last one,
and Down on the last entry jumps to the first, instead of doing nothing.

Only a fresh press wraps. Holding the stick or key still stops at the end, so a
held direction never loops endlessly.

## How it works

`favoritesmenu.swf` carries its own copy of SkyUI's `ScrollingList` (it is not
imported from `inventorylists.swf`), so the patch touches the favorites menu only.
`tools/patch_wrap.py` adds one branch each to `moveSelectionUp` / `moveSelectionDown`
and records in `handleInput` whether the input was `keyDown` or `keyHold`.

## Build

```bash
cd tools
SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh outdir
```

Produces `outdir/Interface/favoritesmenu.swf`. Only `ScrollingList` is re-imported;
every other class keeps SkyUI's original bytecode (a full JPEXS recompile changes
most classes and is known to break list scrolling).

Requires SkyUI 6.x. Install as a loose-file mod; loose files always win over
`SkyUI_SE.bsa`, so no load order is involved.

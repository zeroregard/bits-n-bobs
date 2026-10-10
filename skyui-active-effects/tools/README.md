# Analysis tools

Pure-Python, no third-party deps. Built to inspect SkyUI without a decompiler.

```bash
# list / extract a Skyrim SE BSA
python bsax.py "<path>/SkyUI_SE.bsa"
python bsax.py "<path>/SkyUI_SE.bsa" --extract ./out

# SWF structure + AS2 analysis
python swfas2.py out/interface/magicmenu.swf --symbols
python swfas2.py out/interface/magicmenu.swf --symbol "defines.Magic"
python swfas2.py out/interface/magicmenu.swf --strings effect
python swfas2.py out/interface/magicmenu.swf --per-symbol

# AS3/ABC (proves magicmenu.swf has zero DoABC blocks)
python swfabc.py out/interface/magicmenu.swf --tags
```

Gotcha worth keeping: Skyrim **SE** BSAs (v105) compress with **LZ4 frame**
(magic `04 22 4D 18`), not raw LZ4 block. `lz4f.py` handles both plus zlib.

# Building the mod

```bash
SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh outdir
```
Produces `Interface/magicmenu.swf`, `Interface/SkyUI/inventorylists.swf` and
`Interface/SkyUI/icons_category_psychosteve.swf`.

- `patch_aec.py` — magicmenu.swf: classification (`MagicDataSetter`), column layout and tab icons (`MagicMenu`).
- `patch_aec_lists.py` — inventorylists.swf (magicmenu *imports* `InventoryLists` from it, so it must be patched there): replaces the Active Effects tab with TEMPORAL / HARMFUL / PERKS. Gated on `_global.MagicMenu`, since every item menu shares this file.
- `patch_icons.py` + `../icons/` — adds `aec_temporal`, `aec_harmful`, `aec_perks` frames to the category icon SWF, sized to match `mag_activeeffects`. `icons/build_svgs.py` (skia-python) regenerates the SVGs.

Only the patched classes are re-imported; a full JPEXS round-trip rewrites most
classes and breaks list scrolling. The game re-reads magicmenu.swf and
inventorylists.swf on every menu open; the icon SWF stays cached until restart.

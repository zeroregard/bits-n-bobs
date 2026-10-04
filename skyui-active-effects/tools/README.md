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

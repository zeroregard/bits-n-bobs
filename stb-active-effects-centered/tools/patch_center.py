"""Centre STB Active Effects' horizontal row on its configured X position.

STB anchors its widget at (fPosAeffWidgetX, fPosAeffWidgetY) and grows the row in
one direction, so a row is only centred for one icon count. This patches the
widget class: setPosX remembers the configured X, and after every Update the
widget measures its own bounds and shifts so their middle sits on that X.
Vertical mode is left untouched.

  patch_center.py <exported scripts dir>
"""
import os, sys
P = os.path.join(sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__), "__Packages", "activeEffectsSTBWidget.as")
s = open(P, encoding="utf-8").read()
if "stbBaseX" in s:
    print("already patched"); sys.exit(0)
def sub(old, new, why):
    global s
    if s.count(old) != 1: raise SystemExit("ANCHOR NOT FOUND/UNIQUE for: " + why)
    s = s.replace(old, new); print("  patched activeEffectsSTBWidget.as  " + why)
sub("   function setPosX(widgetX)\n   {\n      this._x = widgetX;\n   }",
    "   function setPosX(widgetX)\n   {\n      this.stbBaseX = widgetX;\n      this._x = widgetX;\n      this.stbCenter();\n   }",
    "remember configured X")
sub("   function Update(a)\n   {\n      this.updateEffects(a);\n   }",
    "   function Update(a)\n   {\n      this.updateEffects(a);\n      this.stbCenter();\n   }\n"
    "   function stbCenter()\n   {\n"
    "      if(this.direction != \"hor\" || this.stbBaseX == undefined)\n      {\n         return undefined;\n      }\n"
    "      var b = this.getBounds(this);\n"
    "      if(b.xMax > b.xMin)\n      {\n"
    "         this._x = this.stbBaseX - (b.xMin + b.xMax) / 2 * this._xscale / 100;\n"
    "      }\n      else\n      {\n         this._x = this.stbBaseX;\n      }\n   }",
    "centre the row after every update")
open(P, "w", encoding="utf-8").write(s)

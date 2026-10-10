"""Patch SkyUI 6.11 favoritesmenu.swf so list navigation wraps around.

Up on the first entry jumps to the last one, Down on the last entry to the first.
Only a fresh press wraps (not a held key), so holding the stick still stops at
the end. favoritesmenu.swf has its own copy of ScrollingList (nothing imports it
from inventorylists.swf), so this affects the favorites menu only.

  patch_wrap.py <exported scripts dir>
"""
import os, sys
P = os.path.join(sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__), "__Packages", "skyui", "components", "list", "ScrollingList.as")
s = open(P, encoding="utf-8").read()
if "_wrapAllowed" in s:
    print("already patched"); sys.exit(0)

def sub(old, new, why):
    global s
    if s.count(old) != 1:
        raise SystemExit("ANCHOR NOT FOUND/UNIQUE for: " + why)
    s = s.replace(old, new); print("  patched ScrollingList.as  " + why)

sub("      if(Shared.GlobalFunc.IsKeyPressed(details))\n      {\n         if(details.navEquivalent == gfx.ui.NavigationCode.UP",
    "      this._wrapAllowed = details.value == \"keyDown\";\n"
    "      if(Shared.GlobalFunc.IsKeyPressed(details))\n      {\n         if(details.navEquivalent == gfx.ui.NavigationCode.UP",
    "remember whether this is a fresh press")

WRAP = ("         else if(this._wrapAllowed && this.getListEnumSize() > 1)\n"
        "         {\n"
        "            this.doSetSelectedIndex(this.getListEnumRelativeIndex(%s),skyui.components.list.BasicList.SELECT_KEYBOARD);\n"
        "            this.isMouseDrivenNav = false;\n"
        "            if(this.isPressOnMove)\n"
        "            {\n"
        "               this.onItemPress();\n"
        "            }\n"
        "         }\n")
TAIL = ("            this.isMouseDrivenNav = false;\n"
        "            if(this.isPressOnMove)\n"
        "            {\n"
        "               this.onItemPress();\n"
        "            }\n"
        "         }\n")
sub("            this.doSetSelectedIndex(this.getListEnumRelativeIndex(- this.scrollDelta),skyui.components.list.BasicList.SELECT_KEYBOARD);\n" + TAIL,
    "            this.doSetSelectedIndex(this.getListEnumRelativeIndex(- this.scrollDelta),skyui.components.list.BasicList.SELECT_KEYBOARD);\n" + TAIL
    + WRAP % "this.getListEnumSize() - 1 - this.getSelectedListEnumIndex()",
    "Up on the first entry wraps to the last")
sub("            this.doSetSelectedIndex(this.getListEnumRelativeIndex(this.scrollDelta),skyui.components.list.BasicList.SELECT_KEYBOARD);\n" + TAIL,
    "            this.doSetSelectedIndex(this.getListEnumRelativeIndex(this.scrollDelta),skyui.components.list.BasicList.SELECT_KEYBOARD);\n" + TAIL
    + WRAP % "- this.getSelectedListEnumIndex()",
    "Down on the last entry wraps to the first")
open(P, "w", encoding="utf-8").write(s)

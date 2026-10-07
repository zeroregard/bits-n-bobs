"""Patch SkyUI 6.11 skyui/inventorylists.swf: Active Effects sub-categories.

magicmenu.swf imports the whole InventoryLists panel (class included) from this
SWF, so the category list must be patched here. The SWF is shared by every item
menu, so everything is gated on _global.MagicMenu (only defined in the magic menu).

  1. drop the engine's own Active Effects category (flag 256), then insert
     Temporal/Harmful/Perks before the divider (or at the end) and clear
     their bits from the engine's own categories (the engine sends "All" as
     0xFFFFFEFF; SkyUI's FILTERFLAG_MAGIC_ALL constant is never used)
  2. re-apply entry.aecFlags around itemList.InvalidateData (engine refreshes reset
     filterFlag, and SkyUI processes each entry only once)
  3. aecRecount(): re-evaluate empty categories; called by MagicDataSetter after
     processing, which may be deferred while the list is suspended
  4. use the Active Effects column layout for the new categories

AEC_DEBUG=1 builds categories that are never hidden (bDontHide).
"""
import os, sys
S = sys.argv[1] if len(sys.argv) > 1 else sys.exit("usage: patch_aec_lists.py <scripts dir>")
full = os.path.join(S, "__Packages", "InventoryLists.as")
s = open(full, encoding="utf-8").read()
if "aecEntries" in s: print("  SKIP (already applied)"); sys.exit(0)
H = "true" if os.environ.get("AEC_DEBUG") else "false"

def rep(old, new, why):
    global s
    if s.count(old) != 1: raise SystemExit("ANCHOR NOT FOUND/UNIQUE for: " + why)
    s = s.replace(old, new, 1); print("  patched InventoryLists.as  " + why)

GATE = "      if(_global.MagicMenu != undefined)\n      {\n"
REAPPLY = (GATE +
    "         var aecR = 0;\n"
    "         while(aecR < this.itemList.entryList.length)\n         {\n"
    "            if(this.itemList.entryList[aecR].aecFlags != undefined)\n            {\n"
    "               this.itemList.entryList[aecR].filterFlag = this.itemList.entryList[aecR].filterFlag | this.itemList.entryList[aecR].aecFlags;\n"
    "            }\n"
    "            aecR = aecR + 1;\n         }\n      }\n")

# 1. categories
anchor = "      if(this._bTabbed)\n      {\n         this.categoryList.selectedIndex = 0;"
rep(anchor, GATE +
    "         var aecM = 0;\n"
    "         while(aecM < this.categoryList.entryList.length)\n         {\n"
    "            this.categoryList.entryList[aecM].flag = this.categoryList.entryList[aecM].flag & -3585;\n"
    "            aecM = aecM + 1;\n         }\n"
    "         var aecD = this.categoryList.entryList.length - 1;\n"
    "         while(aecD >= 0)\n         {\n"
    "            if(this.categoryList.entryList[aecD].flag == 256)\n            {\n"
    "               this.categoryList.entryList.splice(aecD,1);\n"
    "               if(this.categoryList.dividerIndex != undefined && this.categoryList.dividerIndex > aecD)\n               {\n"
    "                  this.categoryList.dividerIndex = this.categoryList.dividerIndex - 1;\n"
    "               }\n"
    "            }\n"
    "            aecD = aecD - 1;\n         }\n"
    "         var aecEntries = ["
    "{text:\"TEMPORAL\",flag:1024,bDontHide:%(h)s,savedItemIndex:0,filterFlag:0},"
    "{text:\"HARMFUL\",flag:512,bDontHide:%(h)s,savedItemIndex:0,filterFlag:0},"
    "{text:\"PERKS\",flag:2048,bDontHide:%(h)s,savedItemIndex:0,filterFlag:0}];\n"
    "         var aecAt = this.categoryList.dividerIndex;\n"
    "         if(aecAt == undefined || aecAt < 0)\n         {\n"
    "            aecAt = this.categoryList.entryList.length;\n         }\n"
    "         else\n         {\n"
    "            this.categoryList.dividerIndex = aecAt + aecEntries.length;\n         }\n"
    "         var aecI = 0;\n"
    "         while(aecI < aecEntries.length)\n         {\n"
    "            this.categoryList.entryList.splice(aecAt + aecI,0,aecEntries[aecI]);\n"
    "            aecI = aecI + 1;\n         }\n"
    "      }\n" % {"h": H} + anchor,
    "magic-menu-gated Temporal/Harmful/Perks, debug=%s" % H)

# 2. re-apply stored classification around the item list refresh
f = s.index("   function InvalidateListData()")
g = s.index("this.itemList.InvalidateData();", f)
ls = s.rindex("\n", 0, g) + 1; le = s.index("\n", g) + 1
s = s[:ls] + REAPPLY + s[ls:le] + REAPPLY + s[le:]
print("  patched InventoryLists.as  re-apply aecFlags around itemList.InvalidateData")

# 3. recount after (deferred) processing
rep("   function InvalidateListData()",
    "   function aecRecount()\n   {\n"
    "      var aecC = 0;\n"
    "      while(aecC < this.categoryList.entryList.length)\n      {\n"
    "         var aecE = this.categoryList.entryList[aecC];\n"
    "         if(aecE.filterFlag == 0 && !aecE.bDontHide)\n         {\n"
    "            var aecK = 0;\n"
    "            while(aecK < this.itemList.entryList.length)\n            {\n"
    "               if(this.itemList.entryList[aecK].filterFlag & aecE.flag)\n               {\n"
    "                  aecE.filterFlag = 1;\n               }\n"
    "               aecK = aecK + 1;\n            }\n"
    "         }\n"
    "         aecC = aecC + 1;\n      }\n"
    "      this.categoryList.UpdateList();\n   }\n"
    "   function InvalidateListData()",
    "aecRecount()")

# 4. column layout
rep("this.itemList.layout.changeFilterFlag(this.categoryList.selectedEntry.flag);",
    "this.itemList.layout.changeFilterFlag((this.categoryList.selectedEntry.flag & 3584) != 0 ? 256 : this.categoryList.selectedEntry.flag);",
    "Active Effects columns for the new categories")
open(full, "w", encoding="utf-8").write(s)
print("all patches applied")

"""Patch SkyUI magicmenu AS2 sources: split Active Effects into sub-categories.

Design (all client-side AS2, no engine cooperation needed):

  * Three new filter bits, 512/1024/2048. Magic categories use bits 0-8, so these
    are free.
  * FILTERFLAG_MAGIC_ALL is ~256 (= -257), i.e. "everything except active effects".
    It must be widened to ~(256|512|1024|2048) = -3841, otherwise effects carrying
    a new bit would leak into the All category.
  * MagicDataSetter ORs the right bit onto each ICT_ACTIVE_EFFECT entry.
      timeRemaining > 0            -> TIMED
      otherwise                    -> PASSIVE
      resistance Disease/Poison    -> HARMFUL (in addition; an effect can be both)
  * SetCategoriesList appends three category entries after the engine's.
    InvalidateListData auto-hides categories with no matching items, so they
    disappear when empty rather than showing empty lists.
  * The original Active Effects category keeps bit 256 and still lists everything.
"""
import os, sys, re

S = sys.argv[1] if len(sys.argv) > 1 else None
if not S:
    sys.exit("usage: patch_aec.py <scripts dir>")

P = os.path.join(S, "__Packages")


def edit(path, old, new, why, sentinel):
    """sentinel must be a string that appears ONLY after this patch is applied."""
    full = os.path.join(P, path)
    s = open(full, encoding="utf-8", errors="replace").read()
    if sentinel in s:
        print("  SKIP (already applied): %s" % why)
        return
    if old not in s:
        raise SystemExit("ANCHOR NOT FOUND in %s for: %s" % (path, why))
    s = s.replace(old, new, 1)
    open(full, "w", encoding="utf-8").write(s)
    after = open(full, encoding="utf-8", errors="replace").read()
    if sentinel not in after:
        raise SystemExit("PATCH DID NOT LAND in %s for: %s" % (path, why))
    print("  patched %-28s %s" % (path, why))


# ---------------------------------------------------------------- 1. constants
edit("skyui/defines/Inventory.as",
     "   static var FILTERFLAG_MAGIC_ALL = -257;",
     "   static var AEC_FLAG_HARMFUL = 512;\n"
     "   static var AEC_FLAG_TIMED = 1024;\n"
     "   static var AEC_FLAG_PASSIVE = 2048;\n"
     "   static var AEC_FLAG_MASK = 3840;\n"
     "   static var FILTERFLAG_MAGIC_ALL = -3841;",
     "new sub-category bits + widened ALL mask", "AEC_FLAG_HARMFUL = 512")

# ------------------------------------------------------- 2. tag entries by kind
edit("MagicDataSetter.as",
     "         case skyui.defines.Inventory.ICT_ACTIVE_EFFECT:\n"
     "            if(a_itemInfo.timeRemaining != undefined && a_itemInfo.timeRemaining > 0)",
     "         case skyui.defines.Inventory.ICT_ACTIVE_EFFECT:\n"
     "            a_entryObject.filterFlag = a_entryObject.filterFlag | "
     "(a_itemInfo.timeRemaining != undefined && a_itemInfo.timeRemaining > 0 "
     "? skyui.defines.Inventory.AEC_FLAG_TIMED : skyui.defines.Inventory.AEC_FLAG_PASSIVE);\n"
     "            if(a_entryObject.resistance == skyui.defines.Actor.AV_DISEASERESIST || "
     "a_entryObject.resistance == skyui.defines.Actor.AV_POISONRESIST)\n"
     "            {\n"
     "               a_entryObject.filterFlag = a_entryObject.filterFlag | "
     "skyui.defines.Inventory.AEC_FLAG_HARMFUL;\n"
     "            }\n"
     "            if(a_itemInfo.timeRemaining != undefined && a_itemInfo.timeRemaining > 0)",
     "tag active-effect entries with sub-category bits", "AEC_FLAG_TIMED :")

# ------------------------------------------------------- 3. append categories
edit("InventoryLists.as",
     "      if(this._bTabbed)\n"
     "      {\n"
     "         this.categoryList.selectedIndex = 0;",
     "      this.categoryList.entryList.push({text:\"Harmful\","
     "flag:skyui.defines.Inventory.AEC_FLAG_HARMFUL,bDontHide:false,savedItemIndex:0,filterFlag:0});\n"
     "      this.categoryList.entryList.push({text:\"Timed\","
     "flag:skyui.defines.Inventory.AEC_FLAG_TIMED,bDontHide:false,savedItemIndex:0,filterFlag:0});\n"
     "      this.categoryList.entryList.push({text:\"Passive\","
     "flag:skyui.defines.Inventory.AEC_FLAG_PASSIVE,bDontHide:false,savedItemIndex:0,filterFlag:0});\n"
     "      if(this._bTabbed)\n"
     "      {\n"
     "         this.categoryList.selectedIndex = 0;",
     "append the three sub-categories", "AEC_FLAG_HARMFUL,bDontHide")

# ------------------------------------------------------------------- 4. icons
edit("MagicMenu.as",
     '"mag_powers","mag_activeeffects"];',
     '"mag_powers","mag_activeeffects","mag_activeeffects","mag_activeeffects","mag_activeeffects"];',
     "icon art for the new categories", '"mag_activeeffects","mag_activeeffects"')

print("all patches applied")

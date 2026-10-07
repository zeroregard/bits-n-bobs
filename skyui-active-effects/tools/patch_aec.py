"""Patch SkyUI 6.11 magicmenu.swf AS2 sources (only classes this SWF really owns).

Classifies each Active Effects entry into one of three sub-categories:
  Temporal 1024  time-limited, or from an enchantment (worn items)
  Harmful   512  the magic effect has the Detrimental flag
  Perks    2048  everything else (abilities, perks, racials, blessings)

Source/flag data comes from the companion SKSE plugin (skse.plugins.AEC, see
../plugin). The magic menu itself only receives an effect's name and type. If the
plugin is missing it falls back to timed -> Temporal, otherwise Perks.

Lessons baked in (see ../README.md):
  * magicmenu.swf imports InventoryLists from skyui/inventorylists.swf, so the
    category list itself is patched by patch_aec_lists.py, not here.
  * itemcard.swf/bottombar.swf also define skyui.defines.Inventory and shadow it,
    so bit values are literals.
  * Entries are processed once and the engine refreshes them, so the result is
    stored in entry.aecFlags and re-applied by the list code.
  * Processing can be deferred while the list is suspended, so after processing
    we ask InventoryLists.aecRecount() to re-evaluate which categories are empty.
"""
import os, sys
S = sys.argv[1] if len(sys.argv) > 1 else sys.exit("usage: patch_aec.py <scripts dir>")
P = os.path.join(S, "__Packages")

def edit(path, old, new, why, sentinel):
    full = os.path.join(P, path); s = open(full, encoding="utf-8").read()
    if sentinel in s: print("  SKIP (already applied): %s" % why); return
    if s.count(old) != 1: raise SystemExit("ANCHOR NOT FOUND/UNIQUE in %s for: %s" % (path, why))
    open(full, "w", encoding="utf-8").write(s.replace(old, new, 1)); print("  patched %-22s %s" % (path, why))

# 1. classification
edit("MagicDataSetter.as",
     "         case skyui.defines.Inventory.ICT_ACTIVE_EFFECT:\n",
     "         case skyui.defines.Inventory.ICT_ACTIVE_EFFECT:\n"
     "            var aecTimed = a_itemInfo.timeRemaining != undefined && a_itemInfo.timeRemaining > 0;\n"
     "            var aecInfo = undefined;\n"
     "            if(this.aecSrc != undefined)\n"
     "            {\n"
     "               aecInfo = this.aecSrc[\"k\" + a_entryObject.formId];\n"
     "               if(aecInfo == undefined) { aecInfo = this.aecSrc[\"n\" + a_entryObject.text]; }\n"
     "            }\n"
     "            if(aecInfo != undefined && aecInfo.det)\n"
     "            {\n"
     "               a_entryObject.aecFlags = 512;\n"
     "            }\n"
     "            else if(aecTimed || aecInfo != undefined && aecInfo.ench)\n"
     "            {\n"
     "               a_entryObject.aecFlags = 1024;\n"
     "            }\n"
     "            else\n"
     "            {\n"
     "               a_entryObject.aecFlags = 2048;\n"
     "            }\n"
     "            a_entryObject.filterFlag = a_entryObject.filterFlag | a_entryObject.aecFlags;\n",
     "classify active effects (Temporal/Harmful/Perks)", "aecFlags = 512")

# 2. fetch plugin data once per pass, then recount categories after (possibly deferred) processing
edit("MagicDataSetter.as",
     "   function processEntry(a_entryObject, a_itemInfo)",
     "   function processList(a_list)\n"
     "   {\n"
     "      this.aecSrc = undefined;\n"
     "      if(skse.plugins.AEC != undefined)\n"
     "      {\n"
     "         var aecArr = [];\n"
     "         skse.plugins.AEC.GetEffectSources(aecArr);\n"
     "         this.aecSrc = {};\n"
     "         var aecI = 0;\n"
     "         while(aecI < aecArr.length)\n"
     "         {\n"
     "            var aecE = aecArr[aecI];\n"
     "            var aecDet = (aecE.effectFlags & 4) != 0;\n"
     "            var aecEnch = aecE.itemType == 21;\n"
     "            var aecKeys = [\"k\" + aecE.mgef, \"k\" + aecE.item, \"n\" + aecE.name];\n"
     "            var aecJ = 0;\n"
     "            while(aecJ < aecKeys.length)\n"
     "            {\n"
     "               var aecR = this.aecSrc[aecKeys[aecJ]];\n"
     "               if(aecR == undefined) { aecR = {det:false,ench:false}; this.aecSrc[aecKeys[aecJ]] = aecR; }\n"
     "               aecR.det = aecR.det || aecDet;\n"
     "               aecR.ench = aecR.ench || aecEnch;\n"
     "               aecJ = aecJ + 1;\n"
     "            }\n"
     "            aecI = aecI + 1;\n"
     "         }\n"
     "      }\n"
     "      super.processList(a_list);\n"
     "      var aecIL = a_list._parent._parent;\n"
     "      if(aecIL.aecRecount != undefined)\n"
     "      {\n"
     "         aecIL.aecRecount();\n"
     "      }\n"
     "   }\n"
     "   function processEntry(a_entryObject, a_itemInfo)",
     "plugin lookup + recount after processing", "GetEffectSources")

# 3. new categories use the Active Effects column layout
edit("MagicMenu.as",
     "_loc5_.changeFilterFlag(this.inventoryLists.categoryList.selectedEntry.flag);",
     "_loc5_.changeFilterFlag((this.inventoryLists.categoryList.selectedEntry.flag & 3584) != 0 ? 256 : this.inventoryLists.categoryList.selectedEntry.flag);",
     "Active Effects columns for the new categories", "& 3584) != 0 ? 256")

# 4. icons
edit("MagicMenu.as",
     '"mag_powers","mag_activeeffects"];',
     '"mag_powers","aec_temporal","aec_harmful","aec_perks","mag_activeeffects"];',
     "icon art for the new categories (frames added by patch_icons.py)", '"aec_temporal"')
print("all patches applied")

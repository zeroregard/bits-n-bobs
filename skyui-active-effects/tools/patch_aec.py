"""Patch SkyUI 6.11 magicmenu.swf AS2 sources (only classes this SWF really owns).

Classifies each Active Effects entry into one of three sub-categories:
  ONGOING EFFECTS 1024  time-limited, or from an enchantment (worn items)
  HARMFUL EFFECTS  512  the magic effect is Hostile or Detrimental (timed or not)
  BOON EFFECTS    2048  everything else (abilities, perks, racials, blessings)

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
     "            var aecDet = (aecE.effectFlags & 5) != 0;\n"
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
# 5. per-effect icons. The plugin's raw fields go on the entry; the mapping lives here so
#    it can be tuned without restarting the game. MagicIconSetter may run before or after
#    MagicDataSetter (processing can be deferred), so both set the label.
edit("MagicDataSetter.as",
     "               if(aecR == undefined) { aecR = {det:false,ench:false}; this.aecSrc[aecKeys[aecJ]] = aecR; }\n",
     "               if(aecR == undefined) { aecR = {det:false,ench:false}; this.aecSrc[aecKeys[aecJ]] = aecR; }\n"
     "               if(aecR.rec == undefined) { aecR.rec = aecE; }\n",
     "keep the plugin record per key", "aecR.rec = aecE")
edit("MagicDataSetter.as",
     "            a_entryObject.filterFlag = a_entryObject.filterFlag | a_entryObject.aecFlags;\n",
     "            a_entryObject.filterFlag = a_entryObject.filterFlag | a_entryObject.aecFlags;\n"
     "            this.aecPickIcon(a_entryObject,aecInfo == undefined ? undefined : aecInfo.rec);\n",
     "pick icon per effect", "this.aecPickIcon(a_entryObject")
ICON_FN = """   function aecPickIcon(a_e, r)
   {
      if(r == undefined)
      {
         return undefined;
      }
      var L = undefined;
      var C = undefined;
      var av = r.primaryAV;
      var st = r.spellType;
      var sl = r.slots;
      var T = "";
      if(r.itemType == 21 && (r.sourceType == 26 || r.sourceType == 41)) { T = "Equipped"; }
      else if(r.itemType == 21) { T = "Enchantment"; }
      else if(r.itemType == 46)
      {
         T = "Potion";
         if((r.alchFlags & 131072) != 0) { T = "Poison"; }
         else if((r.alchFlags & 2) != 0) { T = "Food"; }
      }
      else if(r.itemType == 30) { T = "Ingredient"; }
      else if(r.itemType == 23) { T = "Scroll"; }
      else if(st == 0) { T = "Spell"; }
      else if(st == 1) { T = "Disease"; }
      else if(st == 2 || st == 3) { T = "Power"; }
      else if(st == 5) { T = "Poison"; }
      else if(st == 7) { T = "Shout"; }
      else if(st == 4) { T = "Ability"; }
      if(String(r.name).indexOf("Blessing") >= 0 || String(a_e.text).indexOf("Blessing") >= 0) { T = "Blessing"; }
      a_e.aecType = T;
      if(r.archetype == 46 || r.archetype == 36)
      {
         L = "magic_vampire";
      }
      else if(r.itemType == 46)
      {
         L = "default_potion";
         if((r.alchFlags & 131072) != 0) { L = "potion_poison"; C = 11337907; }
         else if((r.alchFlags & 2) != 0) { L = "default_food"; }
         else if(av == 24) { L = "potion_health"; C = 14364275; }
         else if(av == 25) { L = "potion_magic"; C = 3055579; }
         else if(av == 26) { L = "potion_stam"; C = 5364526; }
         else if(av == 41) { L = "potion_fire"; C = 13055542; }
         else if(av == 42) { L = "potion_shock"; C = 15379200; }
         else if(av == 43) { L = "potion_frost"; C = 2096127; }
      }
      else if(r.itemType == 21 && r.sourceType == 41)
      {
         L = "default_weapon";
      }
      else if(r.itemType == 21 && r.sourceType == 26)
      {
         L = "default_armor";
         if((sl & 64) != 0) { L = "armor_ring"; }
         else if((sl & 32) != 0) { L = "armor_amulet"; }
         else if((sl & 4096) != 0) { L = "armor_circlet"; }
         else if((sl & 512) != 0) { L = "armor_shield"; }
         else if((sl & 4) != 0) { L = "armor_body"; }
         else if((sl & 3) != 0) { L = "armor_head"; }
         else if((sl & 8) != 0) { L = "armor_hands"; }
         else if((sl & 16) != 0) { L = "armor_forearms"; }
         else if((sl & 128) != 0) { L = "armor_feet"; }
      }
      else if(st == 7)
      {
         L = "default_shout";
      }
      else if(st == 1)
      {
         L = "potion_poison";
         C = 7048739;
      }
      if(L == undefined)
      {
         if(r.resist == 41 || av == 41) { L = "magic_fire"; C = 13055542; }
         else if(r.resist == 42 || av == 42) { L = "magic_shock"; C = 15379200; }
         else if(r.resist == 43 || av == 43) { L = "magic_frost"; C = 2096127; }
         else if(r.school == 18) { L = "default_alteration"; }
         else if(r.school == 19) { L = "default_conjuration"; }
         else if(r.school == 20) { L = "default_destruction"; }
         else if(r.school == 21) { L = "default_illusion"; }
         else if(r.school == 22) { L = "default_restoration"; }
         else if(st == 2 || st == 3) { L = "default_power"; }
      }
      if(L != undefined)
      {
         a_e.aecIcon = L;
         a_e.iconLabel = L;
      }
      if(C != undefined)
      {
         a_e.aecColor = C;
         a_e.iconColor = C;
      }
   }
"""
edit("MagicDataSetter.as",
     "   function processList(a_list)\n",
     ICON_FN + "   function processList(a_list)\n",
     "icon + type mapping (school / element / potion / worn slot / shout / disease)", "function aecPickIcon")
edit("MagicIconSetter.as",
     '            a_entryObject.iconLabel = "default_effect";\n',
     '            a_entryObject.iconLabel = a_entryObject.aecIcon != undefined ? a_entryObject.aecIcon : "default_effect";\n'
     '            if(a_entryObject.aecColor != undefined)\n'
     '            {\n'
     '               a_entryObject.iconColor = a_entryObject.aecColor;\n'
     '            }\n',
     "icon setter keeps the per-effect icon", "a_entryObject.aecIcon != undefined")
print("all patches applied")

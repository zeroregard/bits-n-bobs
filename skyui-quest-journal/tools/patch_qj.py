"""Patch SkyUI 6.11 quest_journal.swf: quest subtabs on LB / RB.

  MAIN  main quest, Dawnguard, Dragonborn                      (types 1, 10, 11)
  SIDE  guilds, Companions, Daedric, Civil War, anything else  (types 2-5, 7, 9, ...)
  MISC  the Miscellaneous entry, favors, misc-type quests      (formID 0, types 0, 6, 8)
  DONE  completed quests

The engine fills TitleList.entryList once per journal open, then calls
onQuestsDataComplete; the full list is kept in qjAll and TitleList shows one
subtab. LB/RB (keyboard Q/E) switch. The journal opens on the subtab holding the
selected quest. Top-level tabs keep LT/RT.

  patch_qj.py <exported scripts dir>
"""
import os, sys
P = os.path.join(sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__), "__Packages", "QuestsPage.as")
s = open(P, encoding="utf-8").read()
if "qjApply" in s:
    print("already patched"); sys.exit(0)

def sub(old, new, why):
    global s
    if s.count(old) != 1:
        raise SystemExit("ANCHOR NOT FOUND/UNIQUE for: " + why)
    s = s.replace(old, new); print("  patched QuestsPage.as  " + why)

# 1. after the engine's list is built: keep it, then show one subtab
sub("      this.TitleList.InvalidateData();\n"
    "      this.TitleList.RestoreScrollPosition(_loc11_,true);\n"
    "      this.TitleList.UpdateList();\n"
    "      this.onQuestHighlight();\n"
    "   }\n"
    "   function completedQuestSort",
    "      this.qjAll = [];\n"
    "      var qjI = 0;\n"
    "      while(qjI < this.TitleList.entryList.length)\n"
    "      {\n"
    "         if(!this.TitleList.entryList[qjI].divider)\n"
    "         {\n"
    "            this.qjAll.push(this.TitleList.entryList[qjI]);\n"
    "         }\n"
    "         qjI = qjI + 1;\n"
    "      }\n"
    "      var qjSaved = this.TitleList.entryList[_loc11_];\n"
    "      this.qjTab = qjSaved != undefined ? this.qjTabOf(qjSaved) : 0;\n"
    "      this.qjApply(qjSaved);\n"
    "   }\n"
    "   function qjTabOf(e)\n"
    "   {\n"
    "      if(e.completed)\n"
    "      {\n"
    "         return 3;\n"
    "      }\n"
    "      if(e.formID == 0 || e.type == 0 || e.type == 6 || e.type == 8)\n"
    "      {\n"
    "         return 2;\n"
    "      }\n"
    "      if(e.type == 1 || e.type == 10 || e.type == 11)\n"
    "      {\n"
    "         return 0;\n"
    "      }\n"
    "      return 1;\n"
    "   }\n"
    "   function qjApply(keep)\n"
    "   {\n"
    "      var qjList = [];\n"
    "      var qjSel = 0;\n"
    "      var qjI = 0;\n"
    "      while(qjI < this.qjAll.length)\n"
    "      {\n"
    "         if(this.qjTabOf(this.qjAll[qjI]) == this.qjTab)\n"
    "         {\n"
    "            if(this.qjAll[qjI] == keep)\n"
    "            {\n"
    "               qjSel = qjList.length;\n"
    "            }\n"
    "            qjList.push(this.qjAll[qjI]);\n"
    "         }\n"
    "         qjI = qjI + 1;\n"
    "      }\n"
    "      this.TitleList.entryList.splice(0);\n"
    "      qjI = 0;\n"
    "      while(qjI < qjList.length)\n"
    "      {\n"
    "         this.TitleList.entryList.push(qjList[qjI]);\n"
    "         qjI = qjI + 1;\n"
    "      }\n"
    "      this.qjDrawTabs();\n"
    "      this.TitleList.InvalidateData();\n"
    "      this.TitleList.RestoreScrollPosition(qjSel,true);\n"
    "      this.TitleList.UpdateList();\n"
    "      this.onQuestHighlight();\n"
    "      this.UpdateButtonsVisibility();\n"
    "   }\n"
    "   function qjShift(d)\n"
    "   {\n"
    "      if(this.qjAll == undefined)\n"
    "      {\n"
    "         return undefined;\n"
    "      }\n"
    "      this.qjTab = (this.qjTab + d + 4) % 4;\n"
    "      gfx.io.GameDelegate.call(\"PlaySound\",[\"UIMenuFocus\"]);\n"
    "      this.qjApply(undefined);\n"
    "   }\n"
    "   function qjDrawTabs()\n"
    "   {\n"
    "      if(this.qjTabs == undefined)\n"
    "      {\n"
    "         this.createTextField(\"qjTabs\",this.getNextHighestDepth(),this.TitleList_mc._x,this.TitleList_mc._y - 46,640,40);\n"
    "         this.qjTabs.embedFonts = true;\n"
    "         this.qjTabs.html = true;\n"
    "         this.qjTabs.selectable = false;\n"
    "      }\n"
    "      var qjNames = [\"MAIN\",\"SIDE\",\"MISC\",\"DONE\"];\n"
    "      var qjH = \"<font face='$EverywhereMediumFont' size='24'>\";\n"
    "      var qjI = 0;\n"
    "      while(qjI < 4)\n"
    "      {\n"
    "         var qjN = 0;\n"
    "         var qjJ = 0;\n"
    "         while(qjJ < this.qjAll.length)\n"
    "         {\n"
    "            if(this.qjTabOf(this.qjAll[qjJ]) == qjI)\n"
    "            {\n"
    "               qjN = qjN + 1;\n"
    "            }\n"
    "            qjJ = qjJ + 1;\n"
    "         }\n"
    "         qjH = qjH + \"<font color='\" + (qjI == this.qjTab ? \"#FFFFFF\" : \"#6E6E6E\") + \"'>\" + qjNames[qjI] + \" \" + qjN + \"</font>    \";\n"
    "         qjI = qjI + 1;\n"
    "      }\n"
    "      this.qjTabs.htmlText = qjH + \"</font>\";\n"
    "   }\n"
    "   function completedQuestSort",
    "keep the full list, show one subtab")

# 2. LB/RB (keyboard Q/E) switch subtabs
sub("   function handleInput(details, pathToFocus)\n   {\n",
    "   function handleInput(details, pathToFocus)\n   {\n"
    "      if(Shared.GlobalFunc.IsKeyPressed(details,false))\n"
    "      {\n"
    "         if(details.navEquivalent == gfx.ui.NavigationCode.GAMEPAD_L1 || details.code == 16)\n"
    "         {\n"
    "            this.qjShift(-1);\n"
    "            return true;\n"
    "         }\n"
    "         if(details.navEquivalent == gfx.ui.NavigationCode.GAMEPAD_R1 || details.code == 18)\n"
    "         {\n"
    "            this.qjShift(1);\n"
    "            return true;\n"
    "         }\n"
    "      }\n",
    "LB/RB and Q/E switch subtabs")
open(P, "w", encoding="utf-8").write(s)

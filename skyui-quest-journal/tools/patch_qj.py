"""Patch SkyUI 6.11 quest_journal.swf: quest subtabs on LB / RB.

  MAIN       main quest, Dawnguard, Dragonborn                   (types 1, 10, 11)
  FACTIONS   Mages, Thieves, Dark Brotherhood, Companions, Civil War (types 2-5, 9)
  DAEDRIC    Daedric quests                                          (type 7)
  SIDE       favors, misc-type, the Miscellaneous entry, anything else
  RADIANT    repeatable radiant quests (guild jobs, bounties, favors): started
             by a Story Manager event, not Run Once, type Thieves Guild /
             Companions / Favor. Needs the QuestJournalSubtabs SKSE plugin (skse.plugins.QJS)

Completed quests stay in their category, below a divider after the active ones.

The quest page is shrunk uniformly to free a band at its top (qjLayout, bottom edge
kept); the subtab bar is drawn in that band on the page's parent so it isn't scaled.

Empty subtabs are drawn dark and skipped by LB/RB; the journal never opens on one.

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
    "      if(this.qjCount(this.qjTab) == 0)\n"
    "      {\n"
    "         this.qjTab = this.qjNext(this.qjTab,1);\n"
    "      }\n"
    "      this.qjApply(qjSaved);\n"
    "   }\n"
    "   function qjTabOf(e)\n"
    "   {\n"

    "      if(e.formID != 0 && (e.type == 1 || e.type == 10 || e.type == 11))\n"
    "      {\n"
    "         return 0;\n"
    "      }\n"
    "      if(this.qjIsRadiant(e))\n"
    "      {\n"
    "         return 4;\n"
    "      }\n"
    "      if(e.formID != 0 && (e.type == 2 || e.type == 3 || e.type == 4 || e.type == 5 || e.type == 9))\n"
    "      {\n"
    "         return 1;\n"
    "      }\n"
    "      if(e.formID != 0 && e.type == 7)\n"
    "      {\n"
    "         return 2;\n"
    "      }\n"
    "      return 3;\n"
    "   }\n"
    "   function qjIsRadiant(e)\n"
    "   {\n"
    "      if(e.formID == 0)\n"
    "      {\n"
    "         return false;\n"
    "      }\n"

    "      if(e.qjInfo == undefined)\n"
    "      {\n"
    "         e.qjInfo = skse.plugins.QJS != undefined ? skse.plugins.QJS.GetQuestInfo(e.formID) : -1;\n"
    "      }\n"
    "      if(e.qjInfo < 0)\n"
    "      {\n"
    "         return false;\n"
    "      }\n"
    "      var qjFlags = e.qjInfo % 65536;\n"
    "      var qjType = Math.floor(e.qjInfo / 65536) % 256;\n"
    "      var qjEvent = e.qjInfo >= 16777216;\n"
    "      return qjEvent && (qjFlags & 256) == 0 && (qjType == 3 || qjType == 5 || qjType == 6);\n"
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
    "      var qjActive = false;\n"
    "      var qjDiv = false;\n"
    "      qjI = 0;\n"
    "      while(qjI < qjList.length)\n"
    "      {\n"
    "         if(qjList[qjI].completed && qjActive && !qjDiv)\n"
    "         {\n"
    "            this.TitleList.entryList.push({divider:true,completed:true});\n"
    "            qjDiv = true;\n"
    "            if(qjSel >= qjI)\n"
    "            {\n"
    "               qjSel = qjSel + 1;\n"
    "            }\n"
    "         }\n"
    "         if(!qjList[qjI].completed)\n"
    "         {\n"
    "            qjActive = true;\n"
    "         }\n"
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
    "      var qjT = this.qjNext(this.qjTab,d);\n"
    "      if(qjT == this.qjTab)\n"
    "      {\n"
    "         return undefined;\n"
    "      }\n"
    "      this.qjTab = qjT;\n"
    "      gfx.io.GameDelegate.call(\"PlaySound\",[\"UIMenuFocus\"]);\n"
    "      this.qjApply(undefined);\n"
    "   }\n"
    "   function qjCount(t)\n"
    "   {\n"
    "      var qjN = 0;\n"
    "      var qjI = 0;\n"
    "      while(qjI < this.qjAll.length)\n"
    "      {\n"
    "         if(this.qjTabOf(this.qjAll[qjI]) == t)\n"
    "         {\n"
    "            qjN = qjN + 1;\n"
    "         }\n"
    "         qjI = qjI + 1;\n"
    "      }\n"
    "      return qjN;\n"
    "   }\n"
    "   function qjNext(t, d)\n"
    "   {\n"
    "      var qjS = 1;\n"
    "      while(qjS < 5)\n"
    "      {\n"
    "         var qjC = (t + d * qjS + 10) % 5;\n"
    "         if(this.qjCount(qjC) > 0)\n"
    "         {\n"
    "            return qjC;\n"
    "         }\n"
    "         qjS = qjS + 1;\n"
    "      }\n"
    "      return t;\n"
    "   }\n"
    "   function qjDrawTabs()\n"
    "   {\n"
    "      this.qjLayout();\n"
    "      if(this.qjTabs == undefined)\n"
    "      {\n"
    "         this._parent.createTextField(\"qjTabs\",this._parent.getNextHighestDepth(),this.qjOrigX,this.qjOrigY + 2,this.qjOrigW,this.qjBand);\n"
    "         this.qjTabs = this._parent.qjTabs;\n"
    "         this.qjTabs.embedFonts = true;\n"
    "         this.qjTabs.html = true;\n"
    "         this.qjTabs.selectable = false;\n"
    "      }\n"
    "      var qjNames = [\"MAIN\",\"FACTIONS\",\"DAEDRIC\",\"SIDE\",\"RADIANT\"];\n"
    "      var qjH = \"<p align='center'><font face='$EverywhereMediumFont' size='19'>\";\n"
    "      var qjI = 0;\n"
    "      while(qjI < 5)\n"
    "      {\n"
    "         qjH = qjH + \"<font color='\" + (qjI == this.qjTab ? \"#FFFFFF\" : (this.qjCount(qjI) > 0 ? \"#6E6E6E\" : \"#2E2E2E\")) + \"'>\" + qjNames[qjI] + \"</font>      \";\n"
    "         qjI = qjI + 1;\n"
    "      }\n"
    "      this.qjTabs.htmlText = qjH + \"</font></p>\";\n"
    "   }\n"
    "   function qjLayout()\n"
    "   {\n"
    "      if(this.qjLaid)\n"
    "      {\n"
    "         return undefined;\n"
    "      }\n"
    "      this.qjLaid = true;\n"
    "      this.qjBand = 40;\n"
    "      this.qjOrigX = this._x;\n"
    "      this.qjOrigY = this._y;\n"
    "      this.qjOrigW = this._width;\n"
    "      var qjH0 = this._height;\n"
    "      if(qjH0 <= this.qjBand)\n"
    "      {\n"
    "         return undefined;\n"
    "      }\n"
    "      var qjS = (qjH0 - this.qjBand) / qjH0;\n"
    "      this._xscale = this._xscale * qjS;\n"
    "      this._yscale = this._yscale * qjS;\n"
    "      this._x = this.qjOrigX + this.qjOrigW * (1 - qjS) / 2;\n"
    "      this._y = this.qjOrigY + this.qjBand;\n"
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
# 3. the bar lives on the page's parent, so it doesn't fade with the page: hide/show it
sub("   function endPage()\n   {\n",
    "   function endPage()\n   {\n"
    "      this._parent.qjTabs._visible = false;\n"
    "      this._visible = false;\n",
    "hide subtab bar and page on other journal tabs (scripted transform detaches the page from the fader's timeline)")
sub("   function startPage()\n   {\n",
    "   function startPage()\n   {\n"
    "      this._parent.qjTabs._visible = true;\n"
    "      this._visible = true;\n",
    "show subtab bar and page on the quests tab")
open(P, "w", encoding="utf-8").write(s)

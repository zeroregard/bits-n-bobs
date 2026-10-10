# SkyUI Quest Journal Subtabs

Splits the quest journal's list into subtabs, switched with **LB / RB**
(keyboard **Q / E**). The top-level journal tabs keep LT / RT.

| Subtab | Contains |
|---|---|
| MAIN | main quest, Dawnguard, Dragonborn |
| FACTIONS | Mages, Thieves Guild, Dark Brotherhood, Companions, Civil War |
| DAEDRIC | Daedric quests |
| SIDE | favors, misc-type quests, anything else |
| MISC | one entry per miscellaneous objective, expanded from the Miscellaneous entry |

Completed quests stay in their category's tab, below a divider after the active ones.

Following any MISC entry also turns on the hidden Miscellaneous parent (the vanilla
"show misc objectives" switch); unfollowing the last one turns it off. If the misc
objectives can't be loaded, MISC falls back to the single vanilla Miscellaneous entry.

The journal opens on the subtab holding the selected quest. The quest page is shrunk
to free a band under the journal tabs for the subtab bar.

Patches only `QuestsPage` in SkyUI 6.x's `quest_journal.swf`; everything else keeps
SkyUI's original bytecode. The game reloads the journal SWF every time it opens, so
changes can be tested without restarting.

```bash
cd tools
SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh outdir
```

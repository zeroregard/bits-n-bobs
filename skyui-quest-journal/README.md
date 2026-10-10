# SkyUI Quest Journal Subtabs

Splits the quest journal's list into four subtabs, switched with **LB / RB**
(keyboard **Q / E**). The top-level journal tabs keep LT / RT.

| Subtab | Contains |
|---|---|
| MAIN | main quest, Dawnguard, Dragonborn |
| SIDE | guilds, Companions, Daedric, Civil War, anything else |
| MISC | the Miscellaneous entry, favors, misc-type quests |
| DONE | completed quests |

The journal opens on the subtab holding the selected quest. Each subtab shows its
count in the header.

Patches only `QuestsPage` in SkyUI 6.x's `quest_journal.swf`; everything else keeps
SkyUI's original bytecode. The game reloads the journal SWF every time it opens, so
changes can be tested without restarting.

```bash
cd tools
SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh outdir
```

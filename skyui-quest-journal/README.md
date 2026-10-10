# SkyUI Quest Journal Subtabs

Splits the quest journal's list into subtabs, switched with **LB / RB**
(keyboard **Q / E**). The top-level journal tabs keep LT / RT.

| Subtab | Contains |
|---|---|
| MAIN | main quest, Dawnguard, Dragonborn |
| FACTIONS | Mages, Thieves Guild, Dark Brotherhood, Companions, Civil War |
| DAEDRIC | Daedric quests |
| SIDE | favors, misc-type quests, the Miscellaneous entry, anything else |
| RADIANT | repeatable radiant quests: guild jobs, bounties, radiant favors |

Completed quests stay in their category's tab, below a divider after the active ones.

The journal opens on the subtab holding the selected quest. The quest page is shrunk
to free a band under the journal tabs for the subtab bar.

Patches only `QuestsPage` in SkyUI 6.x's `quest_journal.swf`; everything else keeps
SkyUI's original bytecode. The game reloads the journal SWF every time it opens, so
changes can be tested without restarting.

```bash
cd tools
SKYUI_BSA=".../Data/SkyUI_SE.bsa" FFDEC="java -jar ffdec.jar" ./build.sh outdir
```

## RADIANT and the SKSE plugin

The journal only receives a quest's title, type and state. `plugin/` is a small SKSE
plugin (`QuestJournalSubtabs.dll`, 1.6.1170) exposing `skse.plugins.QJS.GetQuestInfo(formID)`
= quest flags | type << 16 | (started by a Story Manager event) << 24, plus
`GetQuestEvent(formID)` (the event code, e.g. `CLOC`). The rule lives in ActionScript
(`qjIsRadiant`): SM event + not Run Once + type Thieves Guild /
Companions / Favor. Checked against Skyrim.esm: catches TGR*, CR*, BQ*, MGR*, Favor*,
WE*/WI*; excludes DA*, MS*, Run Once dungeon quests and civil war missions. Without
the plugin the tab is simply empty. (A quest's journal `instance` is not a radiant
signal: ordinary quests carry instance > 0 too.)

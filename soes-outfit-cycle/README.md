# SOES Outfit Cycle

One key steps the player through their **favorite** outfits in
[Skyrim Outfit Equipment System NG](https://www.nexusmods.com/skyrimspecialedition/mods/147011),
wrapping around, and shows the outfit name. Default key `'` (DXScanCode 40) - here a
Steam Controller back button (R4) mapped to that key in Steam Input. Ignored while a
menu is open.

- `source/SOESOutfitCycleScript.psc` - the quest script (uses SOES's `ListOutfits(True)`,
  `GetSelectedOutfit`, `SetSelectedOutfit`, `RefreshArmorFor`).
- `build_esp.py` - writes `SOES Outfit Cycle.esp`: one ESL-flagged start-game-enabled
  quest carrying the script (layout copied from SkyUI's script-only quests).
- `stubs/` - minimal headers so the script compiles without the Creation Kit's
  script sources, plus SOES's own native-function header.

Build with [Caprica](https://github.com/Orvid/Caprica) 0.3.0 (`--game=skyrim`); the input
.psc must sit in the working directory, not a subfolder (Caprica treats subfolders as
namespaces, which Skyrim scripts can't have):

```
Caprica.exe --game=skyrim --flags stubs\TESV_Papyrus_Flags.flg --import stubs --output out SOESOutfitCycleScript.psc
python3 build_esp.py "SOES Outfit Cycle.esp"
```

Mod layout: `SOES Outfit Cycle.esp`, `Scripts/SOESOutfitCycleScript.pex`
(+ `Scripts/Source/` copy of the .psc). Requires SOES NG, SKSE.

# STB Active Effects - centred row

Patch for [STB Active Effects](https://www.nexusmods.com/skyrimspecialedition/mods/140002)
2.1: in horizontal mode the row of effect icons stays centred on the configured X,
whatever the number of icons (STB itself anchors the row at one end).

`tools/patch_center.py` edits only the `activeEffectsSTBWidget` class of
`Interface/STBActiveEffects.swf`: `setPosX` remembers the configured X, and after
every `Update` the widget measures its bounds and shifts their middle onto that X.

```bash
java -jar ffdec.jar -export script x STBActiveEffects.swf
python3 tools/patch_center.py x/scripts
mkdir -p only/scripts/__Packages && cp x/scripts/__Packages/activeEffectsSTBWidget.as only/scripts/__Packages/
java -jar ffdec.jar -importScript STBActiveEffects.swf STBActiveEffects.patched.swf only
```

Matching `STB_ActiveEffects.ini` `[Main]` values used here (HUD space is 1280x720):
`iVertAeffWidget = 1` (horizontal), `iSetRightAeffWidget = 1`, `iSetDownAeffWidget = -1`
(extra rows go downward), `fPosAeffWidgetX = 640`, `fPosAeffWidgetY = 75`
(under the compass), `iSetMaxRowAeffWidget = 10`.

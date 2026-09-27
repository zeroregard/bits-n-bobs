# LydiaHuh

A tiny SKSE plugin for Skyrim Special Edition / Anniversary Edition. Lydia sometimes says "Huh?":

- **50% chance every 10 minutes** of unpaused play (timer stops in menus)
- **5% chance whenever she enters combat**

She only says it when she's loaded, alive and within about 57 m of the player. No ESP, no scripts.
Only the vanilla Lydia (`000A2C8E`) is affected.

## Requirements

- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Install

Grab the `LydiaHuh` artifact from the latest [workflow run](../../actions/workflows/lydia-huh.yml).
It holds a single folder, `LydiaHuh/`, which you install as a mod with MO2/Vortex (or copy its contents into `Data/`):

```
LydiaHuh/
├── SKSE/Plugins/LydiaHuh.dll
├── SKSE/Plugins/LydiaHuh.ini
└── Sound/FX/LydiaHuh/huh.wav   <- you supply this
```

### The "Huh?" sound

No audio ships with the mod. Put a PCM `.wav` at `Sound/FX/LydiaHuh/huh.wav`. It has to be there when the game starts.
The sound plays from Lydia's head in 3D.

To use a real Lydia line, extract one from `Skyrim - Voices_en0.bsa`
(`sound/voice/skyrim.esm/femaleeventoned/`) and convert the `.fuz` to `.wav`
(for example with Unfuzer, or `xWMAEncode` plus a WAV converter).

With no `huh.wav`, "Lydia: Huh?" shows up as a notification in the top-left corner instead.

## Config

`SKSE/Plugins/LydiaHuh.ini` sets the interval, both chances, the max distance, volume, sound path and notification mode.
The log is written to `Documents/My Games/Skyrim Special Edition/SKSE/LydiaHuh.log`.

## Build

You need Windows, VS 2022 (C++23) and xmake 3.x.

```
git submodule update --init --recursive
cd LydiaHuh
xmake f -m release
xmake          # -> dist/SKSE/Plugins/LydiaHuh.dll, dist/ is the installable folder
```

CI (`.github/workflows/lydia-huh.yml`) builds the plugin on every push and uploads the `LydiaHuh` mod folder as an artifact.

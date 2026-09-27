# LydiaHuh

A tiny SKSE plugin for Skyrim Special Edition / Anniversary Edition. Lydia sometimes says "Huh?":

- **50% chance every 10 minutes** of unpaused play (timer stops in menus)
- **5% chance whenever she enters combat**

She only says it when she's loaded, alive and within about 57 m of the player. No ESP, no scripts, no audio files.
Only the vanilla Lydia (`000A2C8E`) is affected.

## How she says it

The line comes from the game itself. It's the same "Huh?" she says when you bump into her, in her own voice, with lip-sync and subtitles.
When the mod first needs the line, it searches the vanilla generic dialogue for a line whose text is `Huh?` and whose conditions pass for Lydia.
Nothing is hardcoded by form ID. It then has her say it through Papyrus' `ObjectReference.Say()`, narrowing that topic to just this line for the duration of the call.

The log (`Documents/My Games/Skyrim Special Edition/SKSE/LydiaHuh.log`) lists every matching line it found and its voice file.
If no line is found, "Lydia: Huh?" shows up as a notification instead.

## Requirements

- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Install

Grab the `LydiaHuh` artifact from the latest [workflow run](../../actions/workflows/lydia-huh.yml).
It holds a single folder, `LydiaHuh/`, which you install as a mod with MO2/Vortex (or copy its contents into `Data/`):

```
LydiaHuh/
└── SKSE/Plugins/
    ├── LydiaHuh.dll
    └── LydiaHuh.ini
```

## Config

`SKSE/Plugins/LydiaHuh.ini` sets the interval, both chances, the max distance, the notification mode, and `Line`.
`Line` is the subtitle text to look for, so she can say another of her generic lines (e.g. `Line=What was that?`).

## Build

You need Windows, VS 2022 (C++23) and xmake 3.x.

```
git submodule update --init --recursive
cd LydiaHuh
xmake f -m release
xmake          # -> dist/SKSE/Plugins/LydiaHuh.dll, dist/ is the installable folder
```

CI (`.github/workflows/lydia-huh.yml`) builds the plugin on every push and uploads the `LydiaHuh` mod folder as an artifact.

# Courier Letters Kit

Letters the vanilla courier brings, written and triggered from plain text files. No Creation Kit, no scripting: one INI
file per letter in `Data/SKSE/Plugins/CourierLettersKit/`. When a letter's conditions all hold, the courier finds the
player and hands it over with whatever comes with it. Each letter comes once per character.

Requires SKSE and Address Library. Works on Skyrim SE / AE (1.5.97 and 1.6.x).

## Files

| File | What it is |
|---|---|
| `CourierLettersKit.esp` | ESL: the polling quest and 30 note records (the slots) |
| `SKSE/Plugins/CourierLettersKit.dll` | reads the letter files, decides which is due, gives each note its title and text |
| `Scripts/CLK_QuestScript.pex`, `Scripts/CLK_Native.pex` | the quest that hands due letters to the courier |
| `SKSE/Plugins/CourierLettersKit/*.ini` | your letters (see `examples/`) |

## A letter

```ini
[Letter]
Slot = 2
Title = Letter from an Admirer

[Conditions]
Sex = Female
MinLevel = 10

[Enclosures]
Gold = 250
Item = Skyrim.esm|0x0877B1 1

[Text]
I watched you pass through the market today, {name}.

     - an admirer
```

Section headers (`[Letter]`, `[Conditions]`, `[Enclosures]`) are only for reading; the keys can come in any order. The
one header that means something is `[Any]` (see [NOT and OR](#not-and-or)).
Everything after `[Text]` is the letter, as written (Skyrim's book markup works, e.g. `<font face='$HandwrittenFont'>`).
Lines starting with `;` or `#` are comments, except inside `[Text]`.

## Keys

| Key | Meaning |
|---|---|
| `Slot` | 1 to 30, required: which note record the letter uses. One letter per slot. Never move a delivered letter to another slot. |
| `Enabled` | `false` keeps it from being sent (default `true`) |
| `Title` | the note's name in the inventory |
| `Text` | a one-line text instead of `[Text]` (`\n` for a new line) |
| `Sex` | `Female`, `Male` or `Any` |
| `Race` | race names or editor IDs, comma separated (`Nord, ImperialRace`): one of these |
| `Name` | the player character's name, comma separated: one of these |
| `MinLevel`, `MaxLevel` | the player's level range (0 = no limit) |
| `MinDaysPassed` | game days since the start |
| `AfterLetter` | `Slot` or `Slot Days`: only after that slot's letter was delivered (and that many game days later), comma separated |
| `AfterLetterDays` | the days for `AfterLetter` entries that don't give their own |
| `QuestCompleted` | quests that must be completed: editor ID (`MQ104`) or `Plugin.esp\|0xID`, comma separated |
| `QuestStage` | `Quest Stage`: the quest at that stage or later (or completed), comma separated |
| `Faction` | the player is in these factions (`Plugin.esp\|0xID`; editor IDs need powerofthree's Tweaks) |
| `Global` | `Global Value`: that global at least the value (default 1) |
| `HasItem` | `Item Count`: the player carries at least that many |
| `Item` | `Item Count`: sent along with the letter (any number of these lines' entries, comma separated) |
| `Gold` | gold sent along |
| `Hours` | game hours it may arrive, `18-6` wraps past midnight |
| `Place` | `Outdoors` (default), `Indoors` or `Anywhere` |
| `InCombat` | `true` lets it arrive in combat (default: never) |

Forms are `Plugin.esp|0xID`: the FormID as the Creation Kit or xEdit shows it, without the load-order byte (an ESL's
`FE xxx` prefix too). Text tokens: `{name}` (the player's name), `{race}` (the player's race).

The files are read at every game load, so an edit applies after loading a save.

## NOT and OR

By default every condition must hold, and so must every entry of a comma-separated list (except `Race` and `Name`,
where one entry is enough).

**NOT:** put `!` in front of an entry and it must *not* hold.

```ini
QuestCompleted = MQ104, !MQ105        ; finished Dragon Rising, not yet The Way of the Voice
Faction = !Skyrim.esm|0x048362        ; not in the Companions
HasItem = !Skyrim.esm|0x0877B1        ; doesn't carry the ring
Global = !PlayerIsVampire 1           ; the global is below 1
QuestStage = !MQ105 30                ; hasn't reached stage 30 (and hasn't completed it)
AfterLetter = !4                      ; letter 4 hasn't come (with days: hasn't come that many days ago)
Hours = !9-17                         ; outside 9 to 17
Race = Nord, Imperial, !ImperialRace  ; Race and Name: one of the plain entries, none of the "!" ones
Race = !Argonian, !Khajiit            ; neither
```

It works on every condition key (`Sex = !Male`, `MinLevel = !20` for "below 20"), but not on `Item`, `Gold`,
`AfterLetterDays`, `Place` or `InCombat`.

**OR:** an `[Any]` section holds when *at least one of its lines* holds. A line keeps its usual meaning, so a
comma list in it still needs every entry. Each `[Any]` section must hold, as do the conditions outside them. Any other
header (`[Conditions]`, `[Enclosures]`, ...) ends the `[Any]` section.

```ini
[Conditions]
MinLevel = 10

; finished both MQ104 and MQ105, or is a Companion
[Any]
QuestCompleted = MQ104, MQ105
Faction = Skyrim.esm|0x048362

; and: a Nord, or a woman
[Any]
Race = Nord
Sex = Female
```

`!` and `[Any]` together give the rest: "not (A and B)" is an `[Any]` with `!A` and `!B`. An `AfterLetterDays` inside
an `[Any]` section applies to that section's `AfterLetter` lines; otherwise the one outside applies. Keys that aren't
conditions (`Slot`, `Title`, `Item`, `Gold`, `Place`, ...) inside an `[Any]` section still apply to the whole letter,
with a warning in the log. See `examples/04_either_or.ini`.

## How it decides

Every 30 seconds the quest asks the DLL for the first undelivered letter (lowest slot) whose conditions all hold, then
hands it to the vanilla courier, who walks up as for any other delivery. The save keeps which letters were delivered
and on which day. `CourierLettersKit.log` (next to SKSE's logs) lists the letters read and any mistakes in the files.

Console helpers:
- `cqf CLK_Quest Explain`: writes to the log why each letter is or isn't due: the first condition that fails, as
  written (`waiting (QuestCompleted = !MQ105)`), or for an `[Any]` section the failing entry of each of its lines
  (`waiting ([Any] 1: none of QuestCompleted = MQ105; Faction = Skyrim.esm|0x048362)`)
- `cqf CLK_Quest Resend 3`: forgets that letter 3 was delivered, so it can come again

## Building

- `build.py OUT.esp`: the plugin (reads Skyrim.esm for the note template)
- `tools/compile.py OUTDIR`: the scripts (Caprica under Proton's wine; stubs in `stubs/`, which don't ship)
- `dll/build.sh`: the DLL (clang-cl + xwin, CommonLibSSE-NG)
- `dll/test/run.sh`: tests the letter files' parser and conditions on Linux, without the game (stand-ins for CommonLib)

`TESTING.md` is the in-game test plan, with its letters in `tests/letters/`.

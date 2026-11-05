# Testing NOT and OR (1.1.0)

Version 1.1.0 changes only the DLL: a `!` before a condition entry negates it, and an `[Any]` section holds when at
least one of its lines holds. The plugin and the scripts are the same as 1.0.0, so only the DLL needs rebuilding.

## 0. Without the game

```bash
courier-letters-kit/dll/test/run.sh
```

This compiles `Main.cpp` against stand-ins for CommonLib (`dll/test/PCH.h`). It runs the parser and the conditions
on the examples and on inline cases, and should end with `all passed 0`. It doesn't check the hook, the Papyrus side
or the real CommonLib API. That's what the in-game test is for.

## 1. Build and install

1. `courier-letters-kit/dll/build.sh`. A compile error here, and not in step 0, means the stand-ins differ from the
   real CommonLib API. Fix `Main.cpp` and report what was different.
2. Copy `dll/build/CourierLettersKit.dll` to `Data/SKSE/Plugins/`. `CourierLettersKit.esp` and the `.pex` files
   stay as they are.
3. In `Data/SKSE/Plugins/CourierLettersKit/`, move out any letters already there (the examples, slots 1 to 4, would
   take priority). Copy in `tests/letters/*.ini` (slots 21 to 27).

The log is `CourierLettersKit.log` in SKSE's log folder. Under Proton that's
`steamapps/compatdata/489830/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition/SKSE/`. It's
emptied at each game start. `cqf CLK_Quest Explain` adds one line per letter, in the form `letter N 'Title': ...`.

## 2. The starting state

Use a throwaway save: the test completes quests and changes race, sex and level. Starting from the main menu with
`coc QASmoke` is fine. From the console, put the player in this state:

```
player.setrace NordRace
player.setlevel 1
set TimeScale to 20
set GameHour to 12
player.removefromfaction 48362
player.removeitem 877B1 99
```

The player should also be male (`player.sexchange` if not), with MQ104 and MQ105 not completed.

48362 is CompanionsFaction and 877B1 is the ring from the examples. If the log says `no faction Skyrim.esm|0x048362`,
look the faction up in xEdit and fix the ID in `21_not.ini` and `23_any.ini`.

## 3. The load log

After loading the save, the log should contain:

```
CourierLettersKit 1.1.0 loaded
letter 21: 'Test 21: NOT' (21_not.ini), 5 condition line(s), 0 [Any] section(s), 0 enclosure(s)
letter 22: 'Test 22: Race mix' (22_race_mix.ini), 1 condition line(s), 0 [Any] section(s), 0 enclosure(s)
letter 23: 'Test 23: Any' (23_any.ini), 1 condition line(s), 2 [Any] section(s), 0 enclosure(s)
letter 24: 'Test 24: Any + NOT' (24_any_not.ini), 0 condition line(s), 1 [Any] section(s), 0 enclosure(s)
letter 25: 'Test 25: not after 21' (25_not_after.ini), 1 condition line(s), 0 [Any] section(s), 0 enclosure(s)
letter 26: 'Test 26: a day after 21' (26_after_days.ini), 1 condition line(s), 0 [Any] section(s), 0 enclosure(s)
letter 27: 'Test 27: mistakes' (27_mistakes.ini) - disabled, 1 condition line(s), 0 [Any] section(s), 2 enclosure(s)
7 letter(s) read
```

It should also have these 10 warnings for `27_mistakes.ini`, and none for any other file:

```
27_mistakes.ini: Gold is not a condition: it applies to the whole letter, not to its [Any] section
27_mistakes.ini: AfterLetter: a slot, 1 to 30: 40
27_mistakes.ini: Item: '!' doesn't apply here, ignored
27_mistakes.ini: Faction: no faction NoSuchFaction (by editor ID only with powerofthree's Tweaks; Plugin.esp|0xID always works)
27_mistakes.ini: Sex: Any, Female or Male
27_mistakes.ini: Hours: like 18-6
27_mistakes.ini: unknown key: Frobnicate
27_mistakes.ini: [Any] section 1 has no conditions; ignored
27_mistakes.ini: [Any] section 2 has no conditions; ignored
27_mistakes.ini: AfterLetter = 27: its own slot, so it never comes
```

## 4. Phase A: the conditions (`Explain`, nothing delivered)

**Keep the console open for this whole phase.** The quest hands a due letter to the courier every 30 seconds of
unpaused play, and never while a menu is open. Open the console right after loading. If a letter shows `delivered`
anyway, run `cqf CLK_Quest Resend <slot>` and carry on.

Run the commands of each step, then `cqf CLK_Quest Explain`, and compare the new lines with the expected ones. Each
step builds on the one before. Letter 27 is always `disabled` and 26 always `waiting (AfterLetter = 21 1)`, so they
are left out of the steps below.

| Step | Commands | Expected (21 to 25) |
|---|---|---|
| A1 | none (the starting state) | 21 `due` · 22 `due` · 23 `waiting (MinLevel = 2)` · 24 `due` · 25 `due` |
| A2 | `player.setlevel 2` | 23 `waiting ([Any] 1: none of QuestCompleted = MQ104; Faction = Skyrim.esm\|0x048362)`, the rest as in A1 |
| A3 | `player.addtofaction 48362 0` | 21 `waiting (Faction = !Skyrim.esm\|0x048362)` · 23 `waiting ([Any] 2: none of Hours = 18-6; Sex = Female)` |
| A4 | `set GameHour to 20` | 23 `due` (21 still waiting on Faction) |
| A5 | `set GameHour to 12` · `player.removefromfaction 48362` · `player.additem 877B1 1` | 21 `waiting (HasItem = !Skyrim.esm\|0x0877B1)` · 23 back to the A2 line |
| A6 | `player.removeitem 877B1 1` · `set TimeScale to 30` | 21 `waiting (Global = !TimeScale 30)` |
| A7 | `set TimeScale to 20` · `player.setrace ImperialRace` | 21 `due` · 22 `waiting (Race = Nord, Imperial, !ImperialRace)` |
| A8 | `player.setrace ArgonianRace` | 21 `waiting (Race = !Argonian, !Khajiit)` · 22 as in A7 |
| A9 | `player.setrace NordRace` · `completequest MQ104` | 21 `due` · 22 `due` · 23 `waiting ([Any] 1: none of QuestCompleted = MQ105; Faction = Skyrim.esm\|0x048362)` · 24 `waiting ([Any] 1: none of QuestCompleted = !MQ104; MinLevel = 30)` |
| A10 | `player.setlevel 30` | 24 `due` |
| A11 | `completequest MQ105` | 21 `waiting (QuestCompleted = !MQ105)` · 23 `waiting ([Any] 2: none of Hours = 18-6; Sex = Female)` |
| A12 | `player.sexchange` | 23 `due` |

25 is `due` in every step.

Here's what each step proves:
- A2 to A4: an `[Any]` section needs one of its lines; two `[Any]` sections both must hold. A comma list inside an
  `[Any]` line still needs every entry (A9 and A11).
- A3, A5, A6, A8, A11: `!` on Faction, HasItem, Global, Race and QuestCompleted.
- A7: Race with plain and `!` entries.
- A9 and A10: `!` inside an `[Any]` section.

## 5. Phase B: a real delivery, and AfterLetter with `!` and days

1. Remove `22_race_mix.ini`, `23_any.ini`, `24_any_not.ini` and `27_mistakes.ini` from the letters folder, so only
   21, 25 and 26 are left.
2. Load a save in the starting state (section 2), with the player **outdoors**, for example `coc WhiterunOrigin`.
   Close the console and play unpaused. Changing cells helps the courier find the player.
3. Within a minute or so the log should say `letter 21 ('Test 21: NOT') handed to the courier`. Only 21 is handed
   over: it's due and comes before 25.
4. The courier walks up and hands over **Test 21: NOT**. Read it. The text should be
   `Letter 21 for <player name> the Nord. NOT works.`, not the placeholder text.
5. `cqf CLK_Quest Explain` should show:
   ```
   letter 21 'Test 21: NOT': delivered
   letter 25 'Test 25: not after 21': waiting (AfterLetter = !21)
   letter 26 'Test 26: a day after 21': waiting (AfterLetter = 21 1)
   ```
   25 must never be delivered.
6. Wait 24 hours (the Wait menu, or sleep). After that, `Explain` shows 26 `due`, then the log says
   `letter 26 (...) handed to the courier`, and the courier brings **Test 26: a day after 21**.
7. Run `cqf CLK_Quest Resend 21`, then `Explain`. 21 should be `due`, 25 `due`, and 26 `delivered`.

## 6. Unchanged behaviour

Put the four `examples/*.ini` back, with the test letters removed, and load. The log should list letters 1 to 4 with
no warnings. Examples 1 to 3 use only 1.0.0 syntax, so this checks that old letter files still parse and behave the
same: `Race = Nord, Imperial` is still "one of these", and `AfterLetter` with `AfterLetterDays` still works.

## Reporting back

For each step, report **pass**, or **fail** with the actual log lines. Also report any build errors from step 1 and
any warnings in the log that the steps above don't list.

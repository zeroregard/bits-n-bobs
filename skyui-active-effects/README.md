# SkyUI — Active Effect Categories

Splits SkyUI's single flat **Active Effects** list in the magic menu into
sub-categories: **Harmful**, **Timed**, **Passive**.

Status: **v0.1, built and compiling, not yet verified in game.**

## What it does

SkyUI's magic menu has ten categories, each carrying a filter bit; the item list is
filtered client-side by `entry.filterFlag & category.flag`. This adds three more bits and
three more categories, and tags each active-effect entry as it is processed.

| category | bit | rule |
|---|---|---|
| Harmful | 512 | `resistance` is `AV_DISEASERESIST` (45) or `AV_POISONRESIST` (40) |
| Timed | 1024 | `timeRemaining > 0` |
| Passive | 2048 | no `timeRemaining` |

The original **Active Effects** category is untouched and still lists everything. An effect
can carry more than one bit — a timed disease shows under both Harmful and Timed.

Empty categories hide themselves: SkyUI's `InvalidateListData` already clears a category's
`filterFlag` when no item matches, so Harmful simply does not appear when you are healthy.

## Install

Drop `Interface/magicmenu.swf` into `Data/` (or install `dist/SkyUI-ActiveEffectCategories-0.1.zip`
with a mod manager). A loose file overrides SkyUI's BSA copy.

**Requires SkyUI** (and therefore SKSE). Nothing else.

**This is a replacer** for `magicmenu.swf` — it conflicts with any other mod replacing that
same file. See the assessment for why that was chosen over an Infinity UI injection, and what
it would take to switch.

## Testing

Two builds ship in `dist/`:

- **`magicmenu.baseline.swf`** — decompiled and recompiled **unmodified**. Use this first.
  If the magic menu opens with this, the AS2 round-trip is sound and any later failure is
  our code, not the toolchain.
- **`magicmenu.swf`** — the actual feature.

Failures are silent: a broken SWF means the magic menu simply does not open. Bisecting with
the baseline is the fastest way to tell a toolchain problem from a logic problem.

## Build

```bash
# 1. extract SkyUI's SWF from the BSA
python tools/bsax.py "<game>/Data/SkyUI_SE.bsa" --extract ./out

# 2. decompile
java -jar ffdec.jar -export script ./src ./out/interface/magicmenu.swf

# 3. apply the patches (each one verifies it landed)
python tools/patch_aec.py ./src/scripts

# 4. recompile
java -jar ffdec.jar -importScript ./out/interface/magicmenu.swf ./magicmenu.swf ./src
```

Needs Java (JPEXS/ffdec is a Java app, so this works under Linux natively). Python is
stdlib-only — the BSA and SWF parsers in `tools/` have no third-party dependencies.

`src/scripts/` contains **only the four files that were modified**, not the full 68-file
decompile, so the diff stays readable.

## Known unknowns

- **Not yet run in game.** It compiles and the SWF validates structurally (64/64 symbols
  preserved, all new constants present), but nothing has been loaded by Skyrim.
- **The Harmful rule is unverified.** `resistance` is normalised from `magicType` by
  `fixSKSEExtendedObject`, but I have not observed its value for an active effect at runtime.
  If it turns out unpopulated, Harmful will simply stay empty and the Timed/Passive split —
  which uses `timeRemaining` and *is* confirmed from source — still works.
- Category labels are plain strings, not `$`-prefixed translation keys. Fine for English,
  needs `interface/translations/skyui_se_*.txt` entries for other languages.
- Built against SkyUI 5.2SE. Target runtime is 1.5.97; this was built on a 1.6.1170 machine.
  SkyUI's SWFs are identical across both, but SKSE-extended data is version-specific and
  should be sanity-checked on 1.5.97.

See [`ASSESSMENT.md`](ASSESSMENT.md) for the full feasibility write-up.

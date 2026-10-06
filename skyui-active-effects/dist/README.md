# SkyUI — Active Effect Categories · handoff package

Read this first. It is written for whoever installs and tests this on the Linux /
Steam machine, and it assumes you have not seen the project before.

---

## What this is

A **SkyUI magic-menu modification** that splits the single flat **Active Effects** list
into sub-categories: **Harmful**, **Timed**, **Passive**.

It is a modified `magicmenu.swf` — a Scaleform (Flash) UI file. There is **no ESP, no
script, no SKSE plugin**. Installing it means getting one file to `Data/Interface/magicmenu.swf`.

## Requirements

- **SkyUI** must be installed and enabled. This file *is* SkyUI's own magic menu, modified;
  it needs SkyUI's BSA loaded for `interface/skyui/config.txt`.
- **SKSE** — implied by SkyUI, nothing extra.
- Target runtime: **Skyrim SE 1.5.97**. (It was built on a 1.6.1170 machine. SkyUI's SWFs
  are identical across both, so this should be fine — but it is unverified on 1.5.97, which
  is part of what you are testing.)
- Nothing else. No Infinity UI, no new dependency.

---

## The two archives

| archive | what it is | install this |
|---|---|---|
| `SkyUI-ActiveEffectCategories-BASELINE.zip` | SkyUI's magic menu, decompiled and recompiled **with no changes** | **FIRST** |
| `SkyUI-ActiveEffectCategories-0.1.zip` | the actual feature | second |

**Install BASELINE first.** It should be functionally identical to stock SkyUI. Its only
job is to prove that the decompile→recompile toolchain produces a SWF Skyrim will load.

This matters because **failure is silent**: a bad SWF does not throw an error, the magic
menu simply does not open. Without the baseline you cannot tell "JPEXS broke the file" from
"the feature logic is wrong".

They are the same file (`Interface/magicmenu.swf`) — only one can be active at a time.

---

## Installing

Both archives are already Data-relative with a `fomod/info.xml`:

```
Interface/magicmenu.swf
fomod/info.xml
```

**With Limo (or any mod manager):** install the archive as-is. No folder restructuring
needed. The version shows from `fomod/info.xml`.

**Manually:** extract so the file lands at `<SkyrimData>/Interface/magicmenu.swf`.

A loose `Data/Interface/magicmenu.swf` **overrides** the copy inside `SkyUI_SE.bsa`. That
is the whole mechanism. **To uninstall, delete that one file** — SkyUI's own menu returns.
Nothing inside any BSA is modified.

**Load order:** this must win the file conflict against SkyUI. In Limo that means it is
installed *after* / above SkyUI in the deploy order. If another mod also replaces
`magicmenu.swf`, only one survives — see Compatibility.

---

## Test procedure

Roughly a minute per cycle. Do them in order.

**Step 1 — baseline**
1. Install `...-BASELINE.zip`, make sure it wins over SkyUI
2. Launch through SKSE, load any save
3. Open the magic menu

- **Menu opens, looks like normal SkyUI** → toolchain is good, continue to step 2
- **Menu does not open** → **STOP.** The AS2 round-trip does not survive on this runtime.
  Report this; the approach needs rethinking and nothing below is worth trying.

**Step 2 — the feature**
1. Install `...-0.1.zip` (replacing baseline)
2. Launch, load a save, open the magic menu
3. Look at the **category list down the left side**

**Success looks like:** below the existing `Active Effects` entry there are up to three new
categories — **Harmful**, **Timed**, **Passive**. Selecting one filters the list to just
those effects.

**Categories with nothing in them are hidden on purpose.** If you have no diseases,
`Harmful` will not appear at all. That is correct behaviour, not a failure. To see
something in every category, have at least one active timed effect (a potion or a shrine
blessing) and ideally a disease.

**Also check `Active Effects` itself still lists everything**, and that the other
categories (Alteration, Destruction, Shouts, Powers…) behave normally. A bug in the filter
masks would most likely show up as effects leaking into the **All** category.

---

## What to report back

1. Did the **baseline** magic menu open? (the gate — most important)
2. Did the **feature** magic menu open?
3. Which of Harmful / Timed / Passive appeared, and were their contents right?
4. Did `Active Effects` still show everything?
5. Did anything leak into **All** that should not have?
6. Game version and SkyUI version you tested on.

---

## Known unknowns — read before debugging

- **Nothing here has ever been run in Skyrim.** It compiles, and the rebuilt SWF validates
  structurally (64/64 exported symbols and init blocks preserved, all new constants
  present), but it has not been loaded by the game even once.
- **`Harmful` is the weak link.** It keys off the effect's `resistance` field being
  disease (45) or poison (40). That field is normalised by SkyUI's `fixSKSEExtendedObject`,
  but its runtime value for an active effect was never observed. **If `Harmful` is always
  empty while Timed and Passive work, that is the expected failure** — not a broken build.
  Timed/Passive key off `timeRemaining`, which is confirmed directly from SkyUI's source.
- Category labels are plain English strings, not `$`-prefixed translation keys. Correct in
  English, untranslated elsewhere.

## Compatibility

This is a **replacer**, not an injection. It conflicts with any other mod that replaces
`magicmenu.swf` — typically other SkyUI magic-menu reskins. Last one deployed wins.

It does **not** touch the HUD "active effects" widget (`activeeffects.swf` +
`SKI_ActiveEffectsWidget`), which is a separate SkyUI feature. Mods changing that are fine.

If a conflict turns out to matter, the planned alternative is an Infinity UI injection
(the `alexsylex/LocalMapUpgrade` pattern) which patches at load instead of overwriting —
at the cost of a hard dependency and an SKSE plugin.

---

## Technical notes (only if you need to debug or rebuild)

The magic menu is **ActionScript 2 (AVM1)**, not AS3 — 64 `DoInitAction` tags, zero
`DoABC`. That is why a decompile/recompile round-trip is viable at all.

SkyUI filters categories **client-side**: each category entry carries a `flag`, each item
entry a `filterFlag`, and the list shows an item when `item.filterFlag & category.flag`.
The engine sends categories via the `SetCategoriesList` GameDelegate callback; SkyUI pushes
them into `categoryList.entryList`, and this mod appends three more afterwards.

| category | bit | rule |
|---|---|---|
| Harmful | 512 | `resistance` is 45 (disease) or 40 (poison) |
| Timed | 1024 | `timeRemaining > 0` |
| Passive | 2048 | no `timeRemaining` |

One subtlety that would silently break things: `FILTERFLAG_MAGIC_ALL` is `-257` = `~256`,
meaning "everything except active effects". It was widened to `-3841` = `~(256|512|1024|2048)`.
Without that, every tagged effect leaks into the **All** category.

Full source, build scripts and the feasibility assessment:
<https://github.com/zeroregard/bits-n-bobs> (PR #2, `skyui-active-effects/`).

Rebuilding needs Java (JPEXS/ffdec runs natively on Linux) and stdlib-only Python:

```bash
python tools/bsax.py "<game>/Data/SkyUI_SE.bsa" --extract ./out
java -jar ffdec.jar -export script ./src ./out/interface/magicmenu.swf
python tools/patch_aec.py ./src/scripts
java -jar ffdec.jar -importScript ./out/interface/magicmenu.swf ./magicmenu.swf ./src
```

Gotcha if you touch the BSA tooling: Skyrim **SE** BSAs (v105) compress with **LZ4 frame**
(magic `04 22 4D 18`), not raw LZ4 block.

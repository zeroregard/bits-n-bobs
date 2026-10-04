# Splitting SkyUI's Active Effects into categories — feasibility assessment

First pass. Static analysis only; nothing has been run in game.

**Headline: the approach in the prompt is sound, and cheaper than feared — but one
hard gate is still untested, and one load-bearing fact is still unverified.**

---

## 0. The single most important finding

**SkyUI's `magicmenu.swf` is ActionScript 2 (AVM1), not ActionScript 3.**

```
magicmenu.swf  sig=CWS version=15  bodyLen=185520
tags:  DefineSprite x66 | ExportAssets x64 | DoInitAction x64 | DoAction x2 ...
DoABC blocks: 0
```

`DoInitAction`/`DoAction` are AVM1 tags. There is no ABC/AS3 anywhere in the file.
Skyrim's Scaleform GFx is an AS2 runtime.

This matters because the prompt's main risk — *"decompiled AS3 from optimized bytecode
often looks valid but won't recompile"* — is largely an **AS3** problem. AS3 decompilation
fights an optimizing compiler and a verifier. AS2/AVM1 is a far simpler stack machine,
JPEXS's AS2 support is older and more mature, and the round-trip is correspondingly more
likely to survive.

It is not a guarantee. But the odds are materially better than the prompt assumes.

---

## 1. Does the SWF round-trip? — **NOT YET ANSWERED (the remaining gate)**

I have not tested this, and I want to be blunt about why rather than bury it: round-trip
testing needs **JPEXS/ffdec**, which is a third-party binary download. That is the one
step I did not take unilaterally.

What I can report:

- **Java 21 is installed**, so JPEXS will run (it is a Java app) — on Windows and under
  Linux/Proton, satisfying the Linux constraint.
- The SWF container round-trips fine: it is `CWS` (zlib), decompresses and re-serializes
  cleanly with a ~200-line parser. Container repacking is not a risk.
- The real risk is confined to **AS2 bytecode → source → bytecode** on 64 `DoInitAction`
  blocks.

**Recommended gate, unchanged from the prompt:** decompile, recompile *unmodified*,
drop in as a loose file, confirm the magic menu opens. Until that passes, everything
below is theory.

One refinement: a loose `Data/Interface/magicmenu.swf` overrides the BSA copy, so testing
needs no BSA repacking and is trivially revertible by deleting one file.

---

## 2. Where the category list lives — **localized, and client-side**

This is the good news. The magic menu's categories are **not** driven by the engine in
the way the prompt feared.

| class | role |
|---|---|
| `__Packages.MagicMenu` | owns `categoryList`, `filterFlag`, `changeFilterFlag`, `_categoryListIconArt` |
| `__Packages.CategoryList` | renders the left-hand list (`activeSegment`, `selectorCenter/Left/Right`) |
| `__Packages.skyui.defines.Inventory` | **all** `ICT_*` and `FILTERFLAG_*` constants |
| `__Packages.MagicDataSetter` | builds per-entry display data |
| `__Packages.MagicIconSetter` | per-entry icons |
| `__Packages.skyui.components.list.*` | list/filter framework (`FilteredEnumeration`, `IListProcessor`) |
| `__Packages.skyui.filter.*` | `IFilter`, `ItemTypeFilter`, `SortFilter`, `NameFilter` |

The ten magic categories are a flat constant set:

```
FILTERFLAG_MAGIC_ALL          FILTERFLAG_MAGIC_FAVORITES
FILTERFLAG_MAGIC_ALTERATION   FILTERFLAG_MAGIC_ILLUSION
FILTERFLAG_MAGIC_DESTRUCTION  FILTERFLAG_MAGIC_CONJURATION
FILTERFLAG_MAGIC_RESTORATION  FILTERFLAG_MAGIC_SHOUTS
FILTERFLAG_MAGIC_POWERS       FILTERFLAG_MAGIC_ACTIVEEFFECTS
```

Selecting a category sets a filter flag; the list framework filters the **same** entry
set client-side. The engine sends one flat item list — SkyUI does the categorizing.

`SetCategoriesList` does exist as a GameDelegate callback, but SkyUI builds its own
category list on top. **Adding a category is an AS2-side change**, not an engine change.

**Verdict: adding entries is localized, not threaded through many places.** The surface
is roughly: add filter flag constants, add category list entries + icons, extend the
filter predicate.

---

## 3. What data is reachable for categorizing — **mostly yes, one unknown**

### Confirmed present per entry (`MagicDataSetter`)

```
formId  type  timeRemaining  timeRemainingDisplay  duration  magnitude
magicSchoolName  castLevel  spellCost  castTime  skillLevel
```

**`timeRemaining` / `duration` are populated for active effects.** That alone cleanly
separates *timed* from *passive/permanent* — one of the three requested buckets, free.

### Confirmed present on extended entries (`ItemcardDataExtender`)

```
fixSKSEExtendedObject → processEntry:
    formType (incl. TYPE_EFFECTSETTING = MGEF)
    school  subType  resistance  magicType  flags  bookType
```

Two things worth noting:

- `TYPE_EFFECTSETTING` means the UI layer **does** see MGEF-typed entries.
- `fixSKSEExtendedObject` confirms SkyUI consumes **SKSE-extended** entry objects. SKSE is
  implied by SkyUI, so richer data is available *without* adding a dependency.

### Already-defined but unused constants (`skyui.defines.Magic`)

```
MGEFFLAG_DETRIMENTAL   MGEFFLAG_HOSTILE      MGEFFLAG_RECOVER
MGEFFLAG_DURATION      MGEFFLAG_NODURATION   MGEFFLAG_HIDEINUI
MGEFFLAG_MAGNITUDE     MGEFFLAG_NOAREA       MGEFFLAG_PAINLESS   ... (17 total)
ARCHETYPE_* ... (44 total: VALUEMOD, CUREDISEASE, INVISIBILITY, PARALYSIS, ...)
```

These are **exactly** the categorization axes the prompt wants — detrimental for the
negative bucket, duration flags for the timed bucket, archetypes for finer grain.

**Cross-reference result: no class in `magicmenu.swf` references any `MGEFFLAG_*` or
`ARCHETYPE_*` constant.** They are inherited from the shared SkyUI codebase and sit
unused in this menu.

### The one unverified fact

`ItemcardDataExtender` sets a field literally named `flags`. **I have not proven that
`flags` on an active-effect entry carries the MGEF record flags** (i.e. that
`flags & MGEFFLAG_DETRIMENTAL` is meaningful). It is a strong inference — the constants
exist, the form type is exposed, the field is named `flags`, and it is set in the same
`processEntry` switch that handles `TYPE_EFFECTSETTING` — but it is an inference drawn
from a string table, not from observed runtime data.

**This is the one thing I would verify before writing any feature code**, and it is cheap:
one `Debug`/trace line in AS2 dumping an active-effect entry object, one menu open.

If `flags` turns out to be unpopulated for effects, the fallbacks in descending order are:

1. **Duration-based split only** — timed vs passive. Works today with confirmed data.
2. **Archetype heuristics** via `magicType`/`subType`, if those carry the MGEF archetype.
3. **Name/keyword heuristics** — weak, localization-dependent, I would not ship it.
4. An SKSE plugin feeding the flags in — breaks the SkyUI-only constraint.

---

## 4. Is SkyUI-only achievable? — **probably yes, with one real caveat**

Nothing found so far requires a dependency beyond SkyUI (and therefore SKSE). The
categorization data, the filter framework, the category list and the tab component are
all already in `magicmenu.swf`.

**The caveat is compatibility, not capability.** Shipping a modified `magicmenu.swf` is a
*replacer*: last writer wins, and it conflicts with any other mod replacing the same file.
The prompt's own constraint — "must not break other UI mods" — is in tension with the
SkyUI-only goal.

- **Replacer (SkyUI only):** simplest, no extra dependency, conflicts with other
  `magicmenu.swf` replacers. Given this file is SkyUI's own, the realistic conflict set is
  small (other SkyUI magic-menu reskins), but non-empty.
- **Infinity UI (injection):** avoids the conflict by patching at load, the
  `LocalMapUpgrade` pattern. Costs a hard dependency and an SKSE plugin.

My read: **start as a replacer.** The conflict surface for `magicmenu.swf` specifically is
much narrower than for, say, `hudmenu.swf`, and Infinity UI can be added later without
redoing the AS2 work. Pay that cost when a real conflict appears, not before.

---

## 5. Recommended approach and effort

Phased, each phase gated on the previous:

**Phase 0 — round-trip gate (hard gate, ~1–2 h)**
Install JPEXS. Decompile → recompile unmodified → loose-file drop → open the magic menu.
If this fails, stop and reassess; everything else is void.

**Phase 1 — prove the data (~1–2 h)**
Add a trace dumping one active-effect entry object. Confirm whether `flags` carries MGEF
flags. This decides whether we get three buckets or two.

**Phase 2 — minimum viable split (~1 day)**
Add filter flag constants, extend the category list and its icons, extend the filter
predicate. Start with **two** categories (timed / passive) since that data is confirmed,
and add the detrimental split only once Phase 1 confirms it.

**Phase 3 — polish**
Icons, MCM toggle, ordering, translation strings (`interface/translations/skyui_se_*.txt`
are plain text and already extracted).

**Risks, honestly ranked**

1. AS2 round-trip fails → project dead in current form. *Untested.*
2. `flags` not populated for effects → three buckets become two. *Unverified.*
3. Replacer conflicts with another magic-menu mod → needs Infinity UI. *Deferrable.*
4. Iteration speed — ~1 min/cycle, failures silent. Mitigate by batching and tracing.

---

## Notes on method

Everything above came from parsing the shipped `SkyUI_SE.bsa` directly — no decompiler.
Tools are in `tools/` and are reusable:

- `bsax.py` — Skyrim SE BSA (v105) lister/extractor. Note SE uses **LZ4 frame**
  (magic `04 22 4D 18`), not raw LZ4 block; this trips up naive implementations.
- `lz4f.py` — pure-Python LZ4 frame + block decoder.
- `swfas2.py` — SWF → tag walk → AVM1 `DoInitAction` → `ExportAssets` symbol names and
  `ActionConstantPool` strings, with per-class filtering.
- `swfabc.py` — AS3/ABC equivalent. Kept because it is what proved the file is *not* AS3.

Analysis was done against SkyUI 5.2SE as shipped in `SkyUI_SE.bsa`. The target runtime is
1.5.97; this machine runs 1.6.1170. **SkyUI's SWFs are the same file for both** — Scaleform
assets are not runtime-version-specific — so the structural findings hold. Anything touching
SKSE-extended data should still be confirmed on 1.5.97, since that is version-specific.

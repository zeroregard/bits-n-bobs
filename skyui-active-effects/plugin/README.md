# ActiveEffectCategories SKSE plugin

Exposes `skse.plugins.AEC.GetEffectSources(array)` to Scaleform. For every active
effect on the player it pushes `{mgef, effectFlags, item, itemType, source,
sourceType, duration, inactive, name}`, so the magic menu can tell worn-item
enchantments (Temporal) and detrimental effects (Harmful) apart from permanent ones
(Perks). The menu falls back to timer-only classification when the plugin is absent.

**Skyrim 1.6.1170 only** (`RelocPtr<PlayerCharacter*>(0x031874F8)`, SKSE 2.2.6 layout).

## Build (Linux, cross-compiled)

Needs clang-cl + lld-link (e.g. flatpak `org.kde.Sdk` with the llvm20 extension), the
MSVC CRT/SDK from [xwin](https://github.com/Jake-Shadle/xwin) (`XWIN`, default
`~/opt/xwin`), and next to this file:

- `skse/` — SKSE 2.2.6 source (`skse64/`, `skse64_common/`)
- `common/` — ianpatt/common

```bash
EXTRA="skse/skse64/ScaleformCallbacks.cpp skse/skse64/ScaleformAPI.cpp \
  skse/skse64/ScaleformValue.cpp skse/skse64_common/Relocation.cpp" ./build.sh
```
Output: `ActiveEffectCategories.dll` → `Data/SKSE/Plugins/`.

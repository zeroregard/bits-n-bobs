# SOES NG fix: keep enchantments, no inventory crash

Patch for [Skyrim Outfit Equipment System NG](https://www.nexusmods.com/skyrimspecialedition/mods/147011)
(source: github.com/koukisdevki/SkyrimOutfitEquipmentSystemNG, master 5201a5f).

**Bug.** `RefreshArmorForActor` calls `ActorEquipManager::EquipObject/UnequipObject` with a
null `ExtraDataList`. Enchantments, tempering and the "worn" flag of an item live in that
list, so the engine equips a plain copy of the base form:
- your own enchantments and tempering do nothing while an outfit is active;
- the real piece never counts as worn, so the outfit is re-equipped on every poll;
- re-equipping under an open inventory crashes when sorting
  (`ItemCard::ShowItemData` -> `ExtraDataList::GetMaximumCharge`).

**Fix** (`0001-equip-inventory-instance.patch`, CC-BY-NC-SA-4.0 like upstream):
- pass the inventory instance, preferring worn, then enchanted, then tempered;
- skip the auto-switch poll while the game is paused in a menu.

## Building on Linux (clang-cl + xwin, no MSVC)

`build/` holds what was used: a clang-cl/xwin CMake toolchain, deps built from source
(abseil 20250127.0, protobuf 29.3 static + prebuilt protoc 29.3, spdlog 1.15.1, xbyak,
span-lite, inih r58, rapidcsv, DirectXTK SimpleMath only), and `build-soes.sh`.
CommonLib needs `/Zc:twoPhase-` under clang and `ENABLE_SKYRIM_VR=OFF`.

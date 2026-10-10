// ActiveEffectCategories - SKSE plugin
// Exposes skse.plugins.AEC.GetEffectSources(array) to Scaleform. For every active
// effect on the player it pushes an object describing where the effect comes from,
// which the SkyUI magic menu patch uses to sort effects into ONGOING / HARMFUL / BOON EFFECTS
// and to pick a per-effect icon.
// The magic menu itself only receives an effect's name and type, not its source.
#include "skse64_common/skse_version.h"
#include "skse64_common/Relocation.h"
#include "skse64/PluginAPI.h"
#include "skse64/ScaleformCallbacks.h"
#include "skse64/ScaleformMovie.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameForms.h"
#include "skse64/GameData.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>

// 1.6.1170 address, from SKSE 2.2.6 GameAPI.cpp
static RelocPtr<PlayerCharacter*> s_player(0x031874F8);

// 1.6.1170 address, from SKSE 2.2.6 GameData.cpp
static RelocPtr<DataHandler*> s_dataHandler(0x020F6320);

// Perk entries are polymorphic and SKSE doesn't expose their type, so ability entries
// are recognised by their MSVC RTTI name (vtable[-1] -> CompleteObjectLocator).
static bool HasRTTIName(const void* obj, const char* name)
{
	const UInt64* vtbl = *(const UInt64* const*)obj;
	const UInt32* col = (const UInt32*)vtbl[-1];
	if (!col || col[0] != 1)
		return false;
	uintptr_t base = (uintptr_t)col - col[5];   // pSelf RVA -> image base
	return std::strcmp((const char*)(base + col[3] + 0x10), name) == 0;
}

static bool HasKeyword(const BGSKeywordForm& kf, const char* name);

// Every spell that any perk grants as an ability, and every spell some non-vampire race
// carries (a vampire's race spells outside this set are vampirism). Built once.
static std::unordered_set<UInt32> s_perkSpells;
static std::unordered_set<UInt32> s_mortalRaceSpells;
static bool s_perkSpellsBuilt = false;
static void BuildPerkSpells()
{
	DataHandler* dh = *s_dataHandler;
	if (s_perkSpellsBuilt || !dh)
		return;
	s_perkSpellsBuilt = true;
	for (UInt32 i = 0; i < dh->arrPERK.count; i++) {
		BGSPerk* perk = static_cast<BGSPerk*>(dh->arrPERK.entries[i]);
		if (!perk)
			continue;
		for (UInt32 j = 0; j < perk->perkEntries.count; j++) {
			BGSPerkEntry* pe = perk->perkEntries.entries[j];
			if (pe && HasRTTIName(pe, ".?AVBGSAbilityPerkEntry@@")) {
				SpellItem* sp = static_cast<BGSAbilityPerkEntry*>(pe)->spellItem;
				if (sp)
					s_perkSpells.insert(sp->formID);
			}
		}
	}
	for (UInt32 i = 0; i < dh->races.count; i++) {
		TESRace* r = dh->races.entries[i];
		if (!r || HasKeyword(r->keyword, "Vampire") || !r->spellList.data || !r->spellList.data->spells)
			continue;
		for (UInt32 j = 0; j < r->spellList.data->numSpells; j++)
			if (r->spellList.data->spells[j])
				s_mortalRaceSpells.insert(r->spellList.data->spells[j]->formID);
	}
}

static const char* ModNameOf(UInt32 formID)
{
	DataHandler* dh = *s_dataHandler;
	if (!dh)
		return "";
	tArray<ModInfo*>* lists[2] = { &dh->modList.loadedMods, &dh->modList.loadedCCMods };
	for (auto* l : lists)
		for (UInt32 i = 0; i < l->count; i++)
			if (l->entries[i] && l->entries[i]->IsFormInMod(formID))
				return l->entries[i]->name;
	return "";
}

// Vanilla implements most perk boons as effects inside abilities every race carries
// (PerkSkillBoosts, AlchemySkillBoosts...), each switched on by a HasPerk condition.
// So an effect counts as a perk when its own conditions require a perk.
static const UInt16 kFunction_HasPerk = 448;
static bool PerkConditioned(const MagicItem::EffectItem* ei)
{
	const Condition* c = static_cast<const Condition*>(ei->unk20);
	for (int guard = 0; c && guard < 64; guard++, c = c->next) {
		if (c->functionId != kFunction_HasPerk || (c->comparisonType & Condition::kComparisonFlag_Global))
			continue;
		float v = *reinterpret_cast<const float*>(&c->compareValue);
		UInt8 op = c->comparisonType & 0xE0;
		if ((op == Condition::kComparisonFlag_Equal && v >= 1.0f) ||
			(op == Condition::kComparisonFlag_NotEqual && v == 0.0f) ||
			(op == Condition::kComparisonFlag_Greater && v < 1.0f) ||
			(op == Condition::kComparisonFlag_GreaterEqual && v <= 1.0f && v > 0.0f))
			return true;
	}
	return false;
}

static bool HasKeyword(const BGSKeywordForm& kf, const char* name)
{
	for (UInt32 i = 0; i < kf.numKeywords; i++) {
		const BGSKeyword* k = kf.keywords ? kf.keywords[i] : nullptr;
		if (k && k->keyword.data && _stricmp(k->keyword.data, name) == 0)
			return true;
	}
	return false;
}

static bool ListHas(const TESSpellList& list, const MagicItem* item)
{
	if (!list.data || !list.data->spells)
		return false;
	for (UInt32 i = 0; i < list.data->numSpells; i++)
		if (list.data->spells[i] == item)
			return true;
	return false;
}

// Diagnostics: every menu open rewrites ActiveEffectCategories.log in the SKSE log folder
// with what the plugin saw for each effect.
static FILE* OpenLog()
{
	char path[1024];
	const char* home = std::getenv("USERPROFILE");
	if (!home)
		return nullptr;
	std::snprintf(path, sizeof(path), "%s\\Documents\\My Games\\Skyrim Special Edition\\SKSE\\ActiveEffectCategories.log", home);
	return std::fopen(path, "w");
}

static void SetNum(GFxValue* obj, const char* name, double n) { GFxValue v; v.SetNumber(n); obj->SetMember(name, &v); }

class EffectVisitor
{
	GFxMovieView* view;
	GFxValue* out;
	PlayerCharacter* pc;
	FILE* log;
public:
	EffectVisitor(GFxMovieView* a_view, GFxValue* a_out, PlayerCharacter* a_pc, FILE* a_log) : view(a_view), out(a_out), pc(a_pc), log(a_log) {}
	bool Accept(ActiveEffect* e)
	{
		if (!e || !e->effect || !e->effect->mgef)
			return true;
		EffectSetting* mgef = e->effect->mgef;
		GFxValue obj;
		view->CreateObject(&obj);
		SetNum(&obj, "mgef", mgef->formID);
		SetNum(&obj, "effectFlags", mgef->properties.flags);
		SetNum(&obj, "item", e->item ? e->item->formID : 0);
		SetNum(&obj, "itemType", e->item ? e->item->formType : 0);
		SetNum(&obj, "source", e->sourceItem ? e->sourceItem->formID : 0);
		SetNum(&obj, "sourceType", e->sourceItem ? e->sourceItem->formType : 0);
		SetNum(&obj, "duration", e->duration);
		SetNum(&obj, "inactive", (e->flags & ActiveEffect::kFlag_Inactive) ? 1 : 0);
		SetNum(&obj, "perkCond", PerkConditioned(e->effect) ? 1 : 0);
		// icon inputs; the menu decides the icon so the mapping can change without a restart
		const auto& p = mgef->properties;
		SetNum(&obj, "school", p.school);
		SetNum(&obj, "resist", p.resistance);
		SetNum(&obj, "primaryAV", p.primaryValue);
		SetNum(&obj, "archetype", p.archetype);
		SetNum(&obj, "delivery", p.deliveryType);
		if (e->item && (e->item->formType == kFormType_Spell || e->item->formType == kFormType_ScrollItem))
			SetNum(&obj, "spellType", static_cast<SpellItem*>(e->item)->data.type);
		if (e->item && e->item->formType == kFormType_Potion)
			SetNum(&obj, "alchFlags", static_cast<AlchemyItem*>(e->item)->itemData.flags);
		if (e->sourceItem && e->sourceItem->formType == kFormType_Armor)
			SetNum(&obj, "slots", static_cast<TESObjectARMO*>(e->sourceItem)->bipedObject.data.parts);
		// where the spell lives: 1 = player's race, 2 = granted by a perk, 3 = player's base record
		if (e->item) {
			UInt32 origin = 0;
			if (pc->race && ListHas(pc->race->spellList, e->item))
				origin = 1;
			else if (s_perkSpells.count(e->item->formID))
				origin = 2;
			else if (pc->baseForm && pc->baseForm->formType == kFormType_NPC &&
					 ListHas(static_cast<TESActorBase*>(pc->baseForm)->spellList, e->item))
				origin = 3;
			SetNum(&obj, "origin", origin);
			// 1 = vampire-only race spell, 2 = the player is in a creature form (werewolf, vampire lord)
			UInt32 super = 0;
			if (origin == 1 && HasKeyword(pc->race->keyword, "Vampire") && !s_mortalRaceSpells.count(e->item->formID))
				super = 1;
			else if (origin == 1 && HasKeyword(pc->race->keyword, "ActorTypeCreature"))
				super = 2;
			SetNum(&obj, "supernatural", super);
			GFxValue mod;
			view->CreateString(&mod, ModNameOf(e->item->formID));
			obj.SetMember("modName", &mod);
			if (log) {
				const char* mn = mgef->fullName.name.data;
				const char* inm = e->item->fullName.name.data;
				std::fprintf(log, "mgef %08X %-32s | item %08X type %2u %-36s | spellType %d origin %u perkSet %d perkCond %d super %u flags %08X | %s\n",
					mgef->formID, mn ? mn : "", e->item->formID, e->item->formType, inm ? inm : "",
					(e->item->formType == kFormType_Spell) ? (int)static_cast<SpellItem*>(e->item)->data.type : -1,
					origin, (int)s_perkSpells.count(e->item->formID), PerkConditioned(e->effect) ? 1 : 0, super,
					mgef->properties.flags, ModNameOf(e->item->formID));
			}
			GFxValue itemName;
			const char* in = e->item->fullName.name.data;
			view->CreateString(&itemName, in ? in : "");
			obj.SetMember("itemName", &itemName);
		}
		GFxValue name;
		const char* n = mgef->fullName.name.data;
		view->CreateString(&name, n ? n : "");
		obj.SetMember("name", &name);
		out->PushBack(&obj);
		return true;
	}
};

class GetEffectSources : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args* args)
	{
		if (args->numArgs < 1 || args->args[0].GetType() != GFxValue::kType_Array)
			return;
		PlayerCharacter* pc = *s_player;
		if (!pc)
			return;
		// Call through an opaque pointer so the compiler can't devirtualize: the
		// implementation lives in the game, reached via its vtable.
		MagicTarget* volatile target = &pc->magicTarget;
		tList<ActiveEffect>* effects = target->GetActiveEffects();
		if (!effects)
			return;
		BuildPerkSpells();
		FILE* log = OpenLog();
		if (log) {
			DataHandler* dh = *s_dataHandler;
			std::fprintf(log, "perks in data: %u, perk ability spells: %zu, mortal race spells: %zu, race %08X\n",
				dh ? dh->arrPERK.count : 0, s_perkSpells.size(), s_mortalRaceSpells.size(), pc->race ? pc->race->formID : 0);
		}
		EffectVisitor v(args->movie, &args->args[0], pc, log);
		effects->Visit(v);
		if (log)
			std::fclose(log);
	}
};

static bool RegisterScaleform(GFxMovieView* view, GFxValue* root)
{
	RegisterFunction<GetEffectSources>(root, view, "GetEffectSources");
	return true;
}

extern "C" {
__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
{
	SKSEPluginVersionData::kVersion,
	5,
	"ActiveEffectCategories",
	"zeroregard",
	"",
	0,
	0,
	{ RUNTIME_VERSION_1_6_1170, 0 },
	0,
};

__declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface* skse)
{
	if (skse->isEditor)
		return false;
	auto* sf = (SKSEScaleformInterface*)skse->QueryInterface(kInterface_Scaleform);
	if (!sf)
		return false;
	return sf->Register("AEC", RegisterScaleform);
}
}

// SKSE's ASSERT macro reports through common/IErrors.cpp, which drags in the whole
// logging library. A failed assert here should simply not crash the game.
void _AssertionFailed(const char* file, unsigned long line, const char* desc) {}

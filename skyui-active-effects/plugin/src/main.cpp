// ActiveEffectCategories - SKSE plugin
// Exposes skse.plugins.AEC.GetEffectSources(array) to Scaleform. For every active
// effect on the player it pushes an object describing where the effect comes from,
// which the SkyUI magic menu patch uses to sort effects into Temporal / Harmful / Perks.
// The magic menu itself only receives an effect's name and type, not its source.
#include "skse64_common/skse_version.h"
#include "skse64_common/Relocation.h"
#include "skse64/PluginAPI.h"
#include "skse64/ScaleformCallbacks.h"
#include "skse64/ScaleformMovie.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameForms.h"

// 1.6.1170 address, from SKSE 2.2.6 GameAPI.cpp
static RelocPtr<PlayerCharacter*> s_player(0x031874F8);

static void SetNum(GFxValue* obj, const char* name, double n) { GFxValue v; v.SetNumber(n); obj->SetMember(name, &v); }

class EffectVisitor
{
	GFxMovieView* view;
	GFxValue* out;
public:
	EffectVisitor(GFxMovieView* a_view, GFxValue* a_out) : view(a_view), out(a_out) {}
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
		EffectVisitor v(args->movie, &args->args[0]);
		effects->Visit(v);
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
	1,
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

// QuestJournalSubtabs - SKSE plugin
// Exposes skse.plugins.QJS to the quest journal so it can classify quests by data the
// journal itself never receives. The classification rules live in the journal's
// ActionScript, so they can be tuned without restarting the game.
#include "skse64_common/skse_version.h"
#include "skse64_common/Relocation.h"
#include "skse64/PluginAPI.h"
#include "skse64/ScaleformCallbacks.h"
#include "skse64/ScaleformMovie.h"
#include "skse64/GameForms.h"

// 1.6.1170 address, from SKSE 2.2.6 GameForms.cpp
static RelocAddr<_LookupFormByID> s_lookupFormByID(0x001E01A0);

// TESQuest (CommonLib layout, unchanged SE->AE): QUEST_DATA at 0xD8
// { float delay; u16 flags @0xDC; i8 priority @0xDE; u8 type @0xDF }, event fourcc @0xE0.
static const UInt32 kQuestFlags = 0xDC, kQuestType = 0xDF, kQuestEvent = 0xE0;

static TESForm* Quest(GFxFunctionHandler::Args* args)
{
	if (args->numArgs < 1 || args->args[0].GetType() != GFxValue::kType_Number)
		return nullptr;
	TESForm* f = s_lookupFormByID(static_cast<UInt32>(args->args[0].GetNumber()));
	return (f && f->formType == kFormType_Quest) ? f : nullptr;
}

// GetQuestInfo(formID) -> flags | type << 16 | (started by a Story Manager event) << 24, or -1
class GetQuestInfo : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args* args)
	{
		args->result->SetNumber(-1);
		TESForm* q = Quest(args);
		if (!q)
			return;
		const UInt8* p = reinterpret_cast<const UInt8*>(q);
		UInt32 flags = *reinterpret_cast<const UInt16*>(p + kQuestFlags);
		UInt32 type = p[kQuestType];
		UInt32 ev = *reinterpret_cast<const UInt32*>(p + kQuestEvent);
		bool hasEvent = ev != 0 && ev != 0xFFFFFFFF;
		args->result->SetNumber(flags + type * 65536.0 + (hasEvent ? 16777216.0 : 0.0));
	}
};

// GetQuestEvent(formID) -> the Story Manager event code ("CLOC", "SCPT"...) or ""
class GetQuestEvent : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args* args)
	{
		char code[5] = {};
		TESForm* q = Quest(args);
		if (q) {
			UInt32 ev = *reinterpret_cast<const UInt32*>(reinterpret_cast<const UInt8*>(q) + kQuestEvent);
			if (ev != 0 && ev != 0xFFFFFFFF)
				for (int i = 0; i < 4; i++)
					code[i] = static_cast<char>((ev >> (8 * i)) & 0xFF);
		}
		args->movie->CreateString(args->result, code);
	}
};

static bool RegisterScaleform(GFxMovieView* view, GFxValue* root)
{
	RegisterFunction<GetQuestInfo>(root, view, "GetQuestInfo");
	RegisterFunction<GetQuestEvent>(root, view, "GetQuestEvent");
	return true;
}

extern "C" {
__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
{
	SKSEPluginVersionData::kVersion,
	1,
	"QuestJournalSubtabs",
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
	return sf && sf->Register("QJS", RegisterScaleform);
}
}

void _AssertionFailed(const char* file, unsigned long line, const char* desc) {}

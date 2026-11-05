// Stand-ins for CommonLibSSE-NG / SKSE / spdlog: just enough to compile Main.cpp and test its parser and conditions
// off Windows (dll/test/run.sh). Not the real API: only the members Main.cpp uses, with the same names.
#pragma once
#include <algorithm>
#include <filesystem>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std::literals;

namespace RE {
	using FormID = std::uint32_t;
	struct TESForm {
		virtual ~TESForm() = default;
		std::string edid, name;
		template <class T> T* As() { return dynamic_cast<T*>(this); }
		template <class T> const T* As() const { return dynamic_cast<const T*>(this); }
		const char* GetFormEditorID() const { return edid.c_str(); }
		const char* GetName() const { return name.c_str(); }
		void SetFullName(const char* s) { name = s; }
		static inline std::map<std::string, TESForm*> byEdid;
		static inline std::map<FormID, TESForm*> byId;
		static TESForm* LookupByEditorID(const std::string& e) { auto it = byEdid.find(e); return it == byEdid.end() ? nullptr : it->second; }
		template <class T> static T* LookupByID(FormID id) { auto it = byId.find(id); return it == byId.end() ? nullptr : dynamic_cast<T*>(it->second); }
	};
	struct TESBoundObject : TESForm {};
	struct TESObjectBOOK : TESBoundObject {};
	struct TESQuest : TESForm { bool done = false; std::uint16_t stage = 0; bool IsCompleted() const { return done; } std::uint16_t GetCurrentStageID() const { return stage; } };
	struct TESFaction : TESForm {};
	struct TESGlobal : TESForm { float value = 0; };
	struct TESRace : TESForm {};
	enum class SEX { kMale = 0, kFemale = 1 };
	struct TESNPC : TESForm { SEX sex = SEX::kMale; SEX GetSex() const { return sex; } };
	struct TESObjectCELL { bool interior = false; bool IsInteriorCell() const { return interior; } };
	struct PlayerCharacter {
		std::string name = "Tester"; TESRace* race = nullptr; TESNPC base; std::uint16_t level = 1; bool combat = false;
		TESObjectCELL cell; std::set<TESFaction*> factions; std::map<TESBoundObject*, std::int32_t> inv;
		static inline PlayerCharacter* single = nullptr;
		static PlayerCharacter* GetSingleton() { return single; }
		const char* GetName() const { return name.c_str(); }
		TESRace* GetRace() const { return race; }
		TESNPC* GetActorBase() { return &base; }
		std::uint16_t GetLevel() const { return level; }
		bool IsInCombat() const { return combat; }
		TESObjectCELL* GetParentCell() { return &cell; }
		bool IsInFaction(TESFaction* f) const { return factions.contains(f); }
		std::map<TESBoundObject*, std::int32_t> GetInventoryCounts() const { return inv; }
	};
	struct Calendar { float days = 0, hour = 12; static inline Calendar* single = nullptr; static Calendar* GetSingleton() { return single; }
		float GetDaysPassed() const { return days; } float GetHour() const { return hour; } };
	struct TESFile {};
	struct TESDataHandler {
		static inline std::map<std::pair<std::string, FormID>, TESForm*> forms;
		static TESDataHandler* GetSingleton() { static TESDataHandler d; return &d; }
		TESForm* LookupForm(FormID id, std::string_view plugin) { auto it = forms.find({ std::string(plugin), id }); return it == forms.end() ? nullptr : it->second; }
		template <class T> T* LookupForm(FormID id, std::string_view plugin) { return dynamic_cast<T*>(LookupForm(id, plugin)); }
		const TESFile* LookupModByName(std::string_view) { static TESFile f; return &f; }
	};
	struct BSString { std::string s; BSString(const char* c) : s(c) {} };
	struct ExtraDataList {}; struct TESObjectREFR {}; struct NiPoint3 {}; struct NiMatrix3 {}; struct StaticFunctionTag {};
	namespace BSScript { struct IVirtualMachine { template <class F> void RegisterFunction(const char*, const char*, F) {} }; }
}
namespace REL {
	template <class T> struct Relocation { std::uintptr_t a = 0; Relocation(std::uint64_t) {} std::uintptr_t address() const { return a; } };
	struct Module { static Module& get() { static Module m; return m; } std::uintptr_t base() const { return 0; } };
}
#define RELOCATION_ID(a, b) (b)
namespace SKSE {
	struct LoadInterface {};
	struct Trampoline { void* allocate(std::size_t n) { return std::malloc(n); } template <std::size_t N> void write_branch(std::uintptr_t, std::uintptr_t) {} };
	inline Trampoline& GetTrampoline() { static Trampoline t; return t; }
	inline void Init(const LoadInterface*) {}
	inline void AllocTrampoline(std::size_t) {}
	struct PapyrusInterface { template <class F> void Register(F) {} };
	inline PapyrusInterface* GetPapyrusInterface() { static PapyrusInterface p; return &p; }
	struct MessagingInterface { enum { kDataLoaded, kPreLoadGame, kNewGame, kPostLoadGame }; struct Message { int type; };
		template <class F> void RegisterListener(F) {} };
	inline MessagingInterface* GetMessagingInterface() { static MessagingInterface m; return &m; }
	namespace log {
		inline std::vector<std::string> lines;
		inline std::optional<std::filesystem::path> log_directory() { return std::nullopt; }
		template <class... A> void info(std::format_string<A...> f, A&&... a) { lines.push_back(std::format(f, std::forward<A>(a)...)); }
		template <class... A> void warn(std::format_string<A...> f, A&&... a) { lines.push_back("WARN " + std::format(f, std::forward<A>(a)...)); }
	}
}
namespace spdlog { namespace level { enum { info }; } struct logger { template <class... A> logger(A&&...) {} void set_level(int) {} void flush_on(int) {} };
	namespace sinks { struct basic_file_sink_mt { template <class... A> basic_file_sink_mt(A&&...) {} }; }
	inline void set_default_logger(std::shared_ptr<logger>) {} inline void set_pattern(const char*) {} }
#define SKSEPluginLoad(...) bool PluginLoad(__VA_ARGS__)
namespace logger = SKSE::log;

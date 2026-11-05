// Courier Letters Kit: letters the vanilla courier brings, written and triggered from INI files.
//
// Data/SKSE/Plugins/CourierLettersKit/*.ini, one file per letter (README.md lists every key). The plugin has a pool of
// note records (CourierLettersKit.esp, 0x801.. = slot 1..); a letter's file says which slot it uses. This DLL:
//   - reads the files (at data load and again on every game load, so edits apply after a reload),
//   - names the slot's note after the letter's Title,
//   - gives the note its text when it is read (BookMenu::OpenBookMenu is hooked; the text replaces the record's own),
//   - answers CLK_QuestScript's poll: which letter is due now (all of its conditions hold), and what comes with it.
// What was delivered, and when, is kept by the quest script in the save (no co-save).
#include "PCH.h"

#include <filesystem>

namespace
{
	constexpr auto       kVersion = "CourierLettersKit 1.0.0";
	constexpr auto       kPlugin = "CourierLettersKit.esp";
	constexpr RE::FormID kFirstLetter = 0x801;   // slot 1
	constexpr int        kSlots = 30;
	constexpr RE::FormID kGold = 0x00000F;
	constexpr auto       kFolder = "Data/SKSE/Plugins/CourierLettersKit";

	void SetupLog()
	{
		auto dir = logger::log_directory();
		if (!dir) return;
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "CourierLettersKit.log").string(), true);
		auto log = std::make_shared<spdlog::logger>("log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%m-%d %H:%M:%S] %v");
	}

	std::string Lower(std::string s)
	{
		for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	std::string Trim(const std::string& s)
	{
		const auto a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return {};
		const auto b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	std::vector<std::string> Split(const std::string& s, char sep)
	{
		std::vector<std::string> out;
		std::string              cur;
		std::istringstream       in(s);
		while (std::getline(in, cur, sep))
			if (auto t = Trim(cur); !t.empty()) out.push_back(t);
		return out;
	}

	void ReplaceAll(std::string& s, const std::string& from, const std::string& to)
	{
		for (std::size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
	}

	// "Skyrim.esm|0x0A26E" (any plugin, ESL included; the id without the load-order byte) or an editor ID ("MQ103").
	// The game keeps editor IDs for quests and globals; other kinds need powerofthree's Tweaks for that.
	RE::TESForm* ParseForm(const std::string& s)
	{
		if (const auto bar = s.find('|'); bar != std::string::npos) {
			const auto   plugin = Trim(s.substr(0, bar));
			RE::FormID   local = 0;
			try {
				local = static_cast<RE::FormID>(std::stoul(Trim(s.substr(bar + 1)), nullptr, 16));
			} catch (...) {
				return nullptr;
			}
			auto* dh = RE::TESDataHandler::GetSingleton();
			return dh ? dh->LookupForm(local & 0xFFFFFF, plugin) : nullptr;
		}
		return RE::TESForm::LookupByEditorID(s);
	}

	// "FORM 200": the form, then the number after the last space (a_default when there is none)
	bool ParseFormNumber(const std::string& s, float a_default, RE::TESForm*& a_form, float& a_number)
	{
		a_number = a_default;
		std::string f = Trim(s);
		if (const auto sp = f.find_last_of(" \t"); sp != std::string::npos) {
			try {
				std::size_t used = 0;
				const auto  tail = Trim(f.substr(sp + 1));
				const float n = std::stof(tail, &used);
				if (used == tail.size()) a_number = n, f = Trim(f.substr(0, sp));
			} catch (...) {}
		}
		a_form = ParseForm(f);
		return a_form != nullptr;
	}

	enum class Place { kOutdoors, kIndoors, kAnywhere };

	struct Letter
	{
		std::string file;
		int         slot = 0;
		bool        enabled = true;
		std::string title = "Letter";
		std::string text;
		int         sex = -1;   // -1 any, 0 male, 1 female
		std::vector<std::string> races, names;
		int         minLevel = 0, maxLevel = 0;
		float       minDays = 0.f;
		int         afterLetter = 0;
		float       afterDays = 0.f;
		std::vector<RE::TESQuest*>                       questsDone;
		std::vector<std::pair<RE::TESQuest*, int>>       questStages;
		std::vector<RE::TESFaction*>                     factions;
		std::vector<std::pair<RE::TESGlobal*, float>>    globals;
		std::vector<std::pair<RE::TESBoundObject*, int>> hasItems, items;
		int         hourFrom = -1, hourTo = -1;
		Place       place = Place::kOutdoors;
		bool        inCombat = false;
		std::vector<std::string> problems;
	};

	std::mutex                g_lock;
	std::map<int, Letter>     g_letters;   // by slot

	Letter ParseFile(const std::filesystem::path& p)
	{
		Letter L;
		L.file = p.filename().string();
		std::ifstream in(p, std::ios::binary);
		std::string   line, text;
		bool          inText = false, first = true;
		auto          bad = [&](const std::string& why) { L.problems.push_back(why); };
		while (std::getline(in, line)) {
			if (first && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line = line.substr(3);   // UTF-8 BOM
			first = false;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			const auto t = Trim(line);
			if (inText) {   // everything after [Text] is the letter, as written
				text += line + "\n";
				continue;
			}
			if (t.empty() || t[0] == ';' || t[0] == '#') continue;
			if (Lower(t) == "[text]") { inText = true; continue; }
			if (t[0] == '[') continue;   // [Letter], [Conditions], [Enclosures]: only for reading
			const auto eq = t.find('=');
			if (eq == std::string::npos) { bad("not a key = value line: " + t); continue; }
			const auto key = Lower(Trim(t.substr(0, eq)));
			const auto val = Trim(t.substr(eq + 1));
			auto       num = [&](auto& out) {
                try {
                    out = static_cast<std::remove_reference_t<decltype(out)>>(std::stod(val));
                } catch (...) {
                    bad(key + ": not a number: " + val);
                }
			};
			if (key == "slot") num(L.slot);
			else if (key == "enabled") L.enabled = !(Lower(val) == "false" || val == "0" || Lower(val) == "no");
			else if (key == "title") L.title = val;
			else if (key == "text") { auto v = val; ReplaceAll(v, "\\n", "\n"); text += v + "\n"; }
			else if (key == "sex") {
				const auto v = Lower(val);
				L.sex = v == "female" ? 1 : v == "male" ? 0 : v == "any" || v.empty() ? -1 : (bad("sex: Any, Female or Male"), -1);
			} else if (key == "race") for (auto& r : Split(val, ',')) L.races.push_back(Lower(r));
			else if (key == "name") for (auto& n : Split(val, ',')) L.names.push_back(Lower(n));
			else if (key == "minlevel") num(L.minLevel);
			else if (key == "maxlevel") num(L.maxLevel);
			else if (key == "mindayspassed") num(L.minDays);
			else if (key == "afterletter") num(L.afterLetter);
			else if (key == "afterletterdays") num(L.afterDays);
			else if (key == "questcompleted") {
				for (auto& q : Split(val, ','))
					if (auto* f = ParseForm(q); f && f->As<RE::TESQuest>()) L.questsDone.push_back(f->As<RE::TESQuest>());
					else bad("QuestCompleted: no quest " + q);
			} else if (key == "queststage") {
				for (auto& q : Split(val, ',')) {
					RE::TESForm* f = nullptr; float n = 0;
					if (ParseFormNumber(q, 0, f, n) && f->As<RE::TESQuest>()) L.questStages.push_back({ f->As<RE::TESQuest>(), static_cast<int>(n) });
					else bad("QuestStage: no quest in " + q);
				}
			} else if (key == "faction") {
				for (auto& q : Split(val, ','))
					if (auto* f = ParseForm(q); f && f->As<RE::TESFaction>()) L.factions.push_back(f->As<RE::TESFaction>());
					else bad("Faction: no faction " + q + " (by editor ID only with powerofthree's Tweaks; Plugin.esp|0xID always works)");
			} else if (key == "global") {
				for (auto& q : Split(val, ',')) {
					RE::TESForm* f = nullptr; float n = 1;
					if (ParseFormNumber(q, 1, f, n) && f->As<RE::TESGlobal>()) L.globals.push_back({ f->As<RE::TESGlobal>(), n });
					else bad("Global: no global in " + q);
				}
			} else if (key == "hasitem" || key == "item") {
				for (auto& q : Split(val, ',')) {
					RE::TESForm* f = nullptr; float n = 1;
					if (ParseFormNumber(q, 1, f, n) && f->As<RE::TESBoundObject>())
						(key == "item" ? L.items : L.hasItems).push_back({ f->As<RE::TESBoundObject>(), std::max(1, static_cast<int>(n)) });
					else bad((key == "item" ? "Item" : "HasItem") + std::string(": no item ") + q);
				}
			} else if (key == "gold") {
				int g = 0; num(g);
				if (g > 0)
					if (auto* f = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold)) L.items.push_back({ f, g });
			} else if (key == "hours") {
				const auto parts = Split(val, '-');
				try {
					if (parts.size() == 2) L.hourFrom = std::stoi(parts[0]), L.hourTo = std::stoi(parts[1]);
					else bad("Hours: like 18-6");
				} catch (...) { bad("Hours: like 18-6"); }
			} else if (key == "place") {
				const auto v = Lower(val);
				L.place = v == "indoors" ? Place::kIndoors : v == "anywhere" ? Place::kAnywhere : Place::kOutdoors;
				if (v != "indoors" && v != "anywhere" && v != "outdoors") bad("Place: Outdoors, Indoors or Anywhere");
			} else if (key == "incombat") L.inCombat = Lower(val) == "true" || val == "1" || Lower(val) == "yes";
			else bad("unknown key: " + key);
		}
		while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) text.pop_back();
		L.text = text;
		if (L.slot < 1 || L.slot > kSlots) bad("Slot must be 1 to " + std::to_string(kSlots));
		return L;
	}

	RE::TESObjectBOOK* SlotBook(int slot)
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		return dh ? dh->LookupForm<RE::TESObjectBOOK>(kFirstLetter + slot - 1, kPlugin) : nullptr;
	}

	int SlotOf(const RE::TESObjectBOOK* book)
	{
		if (!book) return 0;
		for (int s = 1; s <= kSlots; ++s)
			if (SlotBook(s) == book) return s;
		return 0;
	}

	std::string Fill(std::string s)
	{
		auto* pc = RE::PlayerCharacter::GetSingleton();
		ReplaceAll(s, "{name}", pc ? pc->GetName() : "");
		ReplaceAll(s, "{race}", pc && pc->GetRace() ? pc->GetRace()->GetName() : "");
		return s;
	}

	void Load()
	{
		std::map<int, Letter> letters;
		std::error_code       ec;
		std::vector<std::filesystem::path> files;
		for (auto& e : std::filesystem::directory_iterator(kFolder, ec))
			if (e.is_regular_file() && Lower(e.path().extension().string()) == ".ini") files.push_back(e.path());
		std::sort(files.begin(), files.end());
		for (auto& f : files) {
			auto L = ParseFile(f);
			for (auto& p : L.problems) logger::warn("{}: {}", L.file, p);
			if (L.slot < 1 || L.slot > kSlots) { logger::warn("{}: skipped (no valid Slot)", L.file); continue; }
			if (letters.contains(L.slot)) { logger::warn("{}: skipped, slot {} is already {}'s", L.file, L.slot, letters[L.slot].file); continue; }
			logger::info("letter {}: '{}' ({}){}, {} enclosure(s)", L.slot, L.title, L.file, L.enabled ? "" : " - disabled", L.items.size());
			letters[L.slot] = std::move(L);
		}
		if (ec) logger::warn("no folder {} ({})", kFolder, ec.message());
		std::scoped_lock lk(g_lock);
		g_letters = std::move(letters);
		logger::info("{} letter(s) read", g_letters.size());
	}

	void ApplyTitles()
	{
		std::scoped_lock lk(g_lock);
		for (auto& [slot, L] : g_letters)
			if (auto* b = SlotBook(slot)) b->SetFullName(Fill(L.title).c_str());
	}

	bool Holds(const Letter& L, const std::vector<float>& delivered, std::string& why)
	{
		auto* pc = RE::PlayerCharacter::GetSingleton();
		auto* cal = RE::Calendar::GetSingleton();
		if (!pc || !cal) return why = "no player", false;
		const float now = cal->GetDaysPassed();
		if (!L.inCombat && pc->IsInCombat()) return why = "in combat", false;
		if (auto* cell = pc->GetParentCell(); cell && L.place != Place::kAnywhere) {
			if (L.place == Place::kOutdoors && cell->IsInteriorCell()) return why = "indoors", false;
			if (L.place == Place::kIndoors && !cell->IsInteriorCell()) return why = "outdoors", false;
		}
		if (L.sex >= 0) {
			auto* base = pc->GetActorBase();
			if (!base || static_cast<int>(base->GetSex()) != L.sex) return why = "sex", false;
		}
		if (!L.races.empty()) {
			auto*       race = pc->GetRace();
			const auto  name = race ? Lower(race->GetName()) : "";
			const auto  edid = race && race->GetFormEditorID() ? Lower(race->GetFormEditorID()) : "";
			if (std::ranges::none_of(L.races, [&](auto& r) { return r == name || r == edid; })) return why = "race", false;
		}
		if (!L.names.empty() && std::ranges::none_of(L.names, [&](auto& n) { return n == Lower(pc->GetName()); })) return why = "name", false;
		const int level = pc->GetLevel();
		if (level < L.minLevel || (L.maxLevel > 0 && level > L.maxLevel)) return why = "level", false;
		if (now < L.minDays) return why = "days passed", false;
		if (L.afterLetter > 0) {
			const auto i = static_cast<std::size_t>(L.afterLetter - 1);
			if (i >= delivered.size() || delivered[i] < 0.f || now - delivered[i] < L.afterDays) return why = "after letter", false;
		}
		for (auto* q : L.questsDone)
			if (!q->IsCompleted()) return why = "quest not completed", false;
		for (auto& [q, stage] : L.questStages)
			if (!q->IsCompleted() && q->GetCurrentStageID() < stage) return why = "quest stage", false;
		for (auto* f : L.factions)
			if (!pc->IsInFaction(f)) return why = "faction", false;
		for (auto& [g, v] : L.globals)
			if (g->value < v) return why = "global", false;
		if (!L.hasItems.empty()) {
			auto inv = pc->GetInventoryCounts();
			for (auto& [obj, n] : L.hasItems)
				if (auto it = inv.find(obj); it == inv.end() || it->second < n) return why = "item", false;
		}
		if (L.hourFrom >= 0) {
			const int h = static_cast<int>(cal->GetHour());
			const bool in = L.hourFrom <= L.hourTo ? (h >= L.hourFrom && h < L.hourTo) : (h >= L.hourFrom || h < L.hourTo);
			if (!in) return why = "hour", false;
		}
		return true;
	}

	// Papyrus: CLK_Native

	std::int32_t NextDue(RE::StaticFunctionTag*, std::vector<float> delivered)
	{
		std::scoped_lock lk(g_lock);
		for (auto& [slot, L] : g_letters) {
			if (!L.enabled || !SlotBook(slot)) continue;
			if (static_cast<std::size_t>(slot - 1) < delivered.size() && delivered[slot - 1] >= 0.f) continue;   // sent
			std::string why;
			if (Holds(L, delivered, why)) return slot;
		}
		return 0;
	}

	RE::TESForm* LetterForm(RE::StaticFunctionTag*, std::int32_t slot) { return SlotBook(slot); }

	std::vector<RE::TESForm*> Items(RE::StaticFunctionTag*, std::int32_t slot)
	{
		std::scoped_lock lk(g_lock);
		std::vector<RE::TESForm*> out;
		if (auto it = g_letters.find(slot); it != g_letters.end())
			for (auto& [obj, n] : it->second.items) out.push_back(obj);
		return out;
	}

	std::vector<std::int32_t> Counts(RE::StaticFunctionTag*, std::int32_t slot)
	{
		std::scoped_lock lk(g_lock);
		std::vector<std::int32_t> out;
		if (auto it = g_letters.find(slot); it != g_letters.end())
			for (auto& [obj, n] : it->second.items) out.push_back(n);
		return out;
	}

	void Delivered(RE::StaticFunctionTag*, std::int32_t slot)
	{
		std::scoped_lock lk(g_lock);
		auto it = g_letters.find(slot);
		logger::info("letter {} ('{}') handed to the courier", slot, it != g_letters.end() ? it->second.title : "?");
	}

	// Why each waiting letter is not due yet, for the log (CLK_Native.Explain() from the console: cgf "CLK_Native.Explain")
	void Explain(RE::StaticFunctionTag*, std::vector<float> delivered)
	{
		std::scoped_lock lk(g_lock);
		for (auto& [slot, L] : g_letters) {
			std::string why;
			const bool  sent = static_cast<std::size_t>(slot - 1) < delivered.size() && delivered[slot - 1] >= 0.f;
			const bool  ok = !sent && L.enabled && Holds(L, delivered, why);
			logger::info("letter {} '{}': {}", slot, L.title, sent ? "delivered" : !L.enabled ? "disabled" : ok ? "due" : "waiting (" + why + ")");
		}
	}

	bool RegisterPapyrus(RE::BSScript::IVirtualMachine* vm)
	{
		vm->RegisterFunction("NextDue", "CLK_Native", NextDue);
		vm->RegisterFunction("Letter", "CLK_Native", LetterForm);
		vm->RegisterFunction("Items", "CLK_Native", Items);
		vm->RegisterFunction("Counts", "CLK_Native", Counts);
		vm->RegisterFunction("Delivered", "CLK_Native", Delivered);
		vm->RegisterFunction("Explain", "CLK_Native", Explain);
		return true;
	}

	// The note's text: BookMenu::OpenBookMenu (ID 51053) gets the text as its first argument; for one of our notes the
	// letter's text goes in instead. Hooked at its entry with the 5-byte branch. Socketing hooks the same function (its
	// ledger), at data load: this one is installed later (first game load) and chains when it finds a branch there.
	namespace BookHook
	{
		using Open_t = void (*)(const RE::BSString&, const RE::ExtraDataList*, RE::TESObjectREFR*, RE::TESObjectBOOK*,
			const RE::NiPoint3&, const RE::NiMatrix3&, float, bool);
		Open_t g_original = nullptr;
		bool   g_installed = false;

		void Thunk(const RE::BSString& a_desc, const RE::ExtraDataList* a_xl, RE::TESObjectREFR* a_ref, RE::TESObjectBOOK* a_book,
			const RE::NiPoint3& a_pos, const RE::NiMatrix3& a_rot, float a_scale, bool a_default)
		{
			if (const int slot = SlotOf(a_book)) {
				std::string text;
				{
					std::scoped_lock lk(g_lock);
					if (auto it = g_letters.find(slot); it != g_letters.end()) text = it->second.text;
				}
				if (!text.empty()) {
					RE::BSString s(Fill(text).c_str());
					return g_original(s, a_xl, a_ref, a_book, a_pos, a_rot, a_scale, a_default);
				}
			}
			g_original(a_desc, a_xl, a_ref, a_book, a_pos, a_rot, a_scale, a_default);
		}

		void Install()
		{
			if (g_installed) return;
			g_installed = true;
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(50122, 51053) };
			const std::uintptr_t            addr = target.address();
			auto*                           p = reinterpret_cast<const std::uint8_t*>(addr);
			auto&                           tramp = SKSE::GetTrampoline();
			// whole instructions at the entry, at least the 5 bytes the branch overwrites: push rbx, rsi, rdi, r14 up to 1.6;
			// push rsi, rdi, r12 on 1.7; or another mod's jmp rel32 (then our original is where that jmp went)
			static constexpr std::uint8_t kPrologue16[6] = { 0x40, 0x53, 0x56, 0x57, 0x41, 0x56 };
			static constexpr std::uint8_t kPrologue17[5] = { 0x40, 0x56, 0x57, 0x41, 0x54 };
			if (p[0] == 0xE9) {
				std::int32_t rel;
				std::memcpy(&rel, p + 1, 4);
				g_original = reinterpret_cast<Open_t>(addr + 5 + rel);
				logger::info("book hook: OpenBookMenu already hooked by another mod - chained");
			} else {
				const std::uint8_t* prologue = nullptr;
				std::size_t         n = 0;
				if (std::memcmp(p, kPrologue16, sizeof kPrologue16) == 0) prologue = kPrologue16, n = sizeof kPrologue16;
				else if (std::memcmp(p, kPrologue17, sizeof kPrologue17) == 0) prologue = kPrologue17, n = sizeof kPrologue17;
				else {
					logger::warn("book hook: OpenBookMenu does not start as expected - not hooked (letters show their placeholder text)");
					return;
				}
				auto* stub = static_cast<std::uint8_t*>(tramp.allocate(n + 14));
				std::memcpy(stub, prologue, n);
				const std::uint8_t jmp[6] = { 0xFF, 0x25, 0, 0, 0, 0 };   // jmp [rip+0] -> addr + n
				std::memcpy(stub + n, jmp, 6);
				const std::uintptr_t back = addr + n;
				std::memcpy(stub + n + 6, &back, 8);
				g_original = reinterpret_cast<Open_t>(stub);
			}
			tramp.write_branch<5>(addr, reinterpret_cast<std::uintptr_t>(&Thunk));
			logger::info("book hook installed (OpenBookMenu @ {:X})", addr - REL::Module::get().base());
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse);
	SetupLog();
	logger::info("{} loaded", kVersion);
	SKSE::AllocTrampoline(64);
	SKSE::GetPapyrusInterface()->Register(RegisterPapyrus);
	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* m) {
		switch (m->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			if (!RE::TESDataHandler::GetSingleton()->LookupModByName(kPlugin)) logger::warn("{} is not loaded", kPlugin);
			Load();
			ApplyTitles();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			BookHook::Install();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			Load();   // edits to the files apply on every load
			ApplyTitles();
			break;
		default:
			break;
		}
		if (m->type == SKSE::MessagingInterface::kNewGame) ApplyTitles();
	});
	return true;
}

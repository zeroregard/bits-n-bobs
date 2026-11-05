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
#include <optional>

namespace
{
	constexpr auto       kVersion = "CourierLettersKit 1.1.0";
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

	// One condition: one entry of a condition line ("QuestCompleted = MQ104, !MQ105" is two). A "!" before an entry turns
	// it around. Race and Name are the exception: their line is one condition (one of its plain entries, none of its "!" ones).
	enum class Kind { kSex, kRace, kName, kMinLevel, kMaxLevel, kMinDays, kAfterLetter, kQuestDone, kQuestStage, kFaction, kGlobal, kHasItem, kHours };

	struct Cond
	{
		Kind                     kind = Kind::kSex;
		bool                     neg = false;
		int                      a = 0, b = 0;   // sex; level; letter slot; quest stage; item count; hours from, to
		float                    f = 0.f;        // days passed; global value; days after the letter (below 0: AfterLetterDays)
		RE::TESForm*             form = nullptr;
		std::vector<std::string> any, none;      // race / name, lower case: one of these, none of those
		std::string              text;           // as written ("QuestCompleted = !MQ105"), for Explain
	};

	// Condition lines. In the letter's own conditions every line must hold; in an [Any] section at least one line must.
	// Within a line, every entry must hold.
	struct Group
	{
		std::vector<std::vector<Cond>> lines;
		float                          afterDays = -1.f;   // AfterLetterDays given in this section (below 0: none)
	};

	struct Letter
	{
		std::string        file;
		int                slot = 0;
		bool               enabled = true;
		std::string        title = "Letter";
		std::string        text;
		Group              all;   // conditions outside [Any] sections
		std::vector<Group> any;   // one per [Any] section
		std::vector<std::pair<RE::TESBoundObject*, int>> items;
		Place              place = Place::kOutdoors;
		bool               inCombat = false;
		std::vector<std::string> problems;
	};

	std::mutex                g_lock;
	std::map<int, Letter>     g_letters;   // by slot

	// "!x" -> (true, "x")
	std::pair<bool, std::string> Negated(const std::string& s)
	{
		if (!s.empty() && s[0] == '!') return { true, Trim(s.substr(1)) };
		return { false, s };
	}

	Letter ParseFile(const std::filesystem::path& p)
	{
		Letter L;
		L.file = p.filename().string();
		std::ifstream in(p, std::ios::binary);
		std::string   line, text;
		bool          inText = false, first = true;
		int           anyIndex = -1;   // the [Any] section being read, -1 outside one
		auto          bad = [&](const std::string& why) { L.problems.push_back(why); };
		auto          scope = [&]() -> Group& { return anyIndex < 0 ? L.all : L.any[anyIndex]; };
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
			if (t[0] == '[') {   // [Any] starts a group of alternatives; the others ([Letter], [Conditions], ...) are only for reading
				const auto close = t.find(']');
				if (Lower(Trim(t.substr(1, close == std::string::npos ? std::string::npos : close - 1))) == "any") {
					L.any.emplace_back();
					anyIndex = static_cast<int>(L.any.size()) - 1;
				} else
					anyIndex = -1;
				continue;
			}
			const auto eq = t.find('=');
			if (eq == std::string::npos) { bad("not a key = value line: " + t); continue; }
			const auto rawKey = Trim(t.substr(0, eq));
			const auto key = Lower(rawKey);
			const auto val = Trim(t.substr(eq + 1));
			const bool inAny = anyIndex >= 0;
			auto       num = [&](const std::string& v, auto& out) {
				try {
					out = static_cast<std::remove_reference_t<decltype(out)>>(std::stod(v));
					return true;
				} catch (...) {
					bad(rawKey + ": not a number: " + v);
					return false;
				}
			};
			auto letterWide = [&] {   // a key that isn't a condition, inside an [Any] section
				if (inAny) bad(rawKey + " is not a condition: it applies to the whole letter, not to its [Any] section");
			};
			auto noNot = [&] {
				if (!val.empty() && val[0] == '!') bad(rawKey + ": '!' doesn't apply here, ignored");
			};
			std::vector<Cond> conds;   // this line's conditions
			auto cond = [&](Kind k, const std::string& entry, std::string& rest) -> Cond& {
				auto [neg, r] = Negated(entry);
				rest = r;
				auto& c = conds.emplace_back();
				c.kind = k, c.neg = neg, c.text = rawKey + " = " + entry;
				return c;
			};
			std::string v;
			if (key == "slot") letterWide(), num(val, L.slot);
			else if (key == "enabled") letterWide(), L.enabled = !(Lower(val) == "false" || val == "0" || Lower(val) == "no");
			else if (key == "title") letterWide(), L.title = val;
			else if (key == "text") { letterWide(); auto s = val; ReplaceAll(s, "\\n", "\n"); text += s + "\n"; }
			else if (key == "place") {
				letterWide();
				const auto s = Lower(val);
				L.place = s == "indoors" ? Place::kIndoors : s == "anywhere" ? Place::kAnywhere : Place::kOutdoors;
				if (s != "indoors" && s != "anywhere" && s != "outdoors") bad("Place: Outdoors, Indoors or Anywhere");
			} else if (key == "incombat") letterWide(), L.inCombat = Lower(val) == "true" || val == "1" || Lower(val) == "yes";
			else if (key == "item") {
				letterWide(), noNot();
				for (auto& q : Split(val, ',')) {
					RE::TESForm* f = nullptr; float n = 1;
					if (ParseFormNumber(Negated(q).second, 1, f, n) && f->As<RE::TESBoundObject>()) L.items.push_back({ f->As<RE::TESBoundObject>(), std::max(1, static_cast<int>(n)) });
					else bad("Item: no item " + q);
				}
			} else if (key == "gold") {
				letterWide(), noNot();
				int g = 0; num(Negated(val).second, g);
				if (g > 0)
					if (auto* f = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold)) L.items.push_back({ f, g });
			} else if (key == "afterletterdays") {
				noNot();
				num(Negated(val).second, scope().afterDays);
			} else if (key == "sex") {
				auto& c = cond(Kind::kSex, val, v);
				const auto s = Lower(v);
				if (s == "female" || s == "male") c.a = s == "female" ? 1 : 0;
				else {
					if (s != "any" && !s.empty()) bad("Sex: Any, Female or Male");
					conds.pop_back();
				}
			} else if (key == "race" || key == "name") {
				const auto k = key == "race" ? Kind::kRace : Kind::kName;
				Cond c;
				c.kind = k, c.text = rawKey + " = " + val;
				for (auto& e : Split(val, ',')) {
					auto [neg, r] = Negated(e);
					(neg ? c.none : c.any).push_back(Lower(r));
				}
				Cond* into = nullptr;   // outside [Any], all Race (Name) lines are one list, as they always were
				if (!inAny)
					for (auto& l : L.all.lines)
						for (auto& o : l)
							if (o.kind == k) into = &o;
				if (into) {
					into->any.insert(into->any.end(), c.any.begin(), c.any.end());
					into->none.insert(into->none.end(), c.none.begin(), c.none.end());
					into->text += ", " + val;
				} else if (!c.any.empty() || !c.none.empty())
					conds.push_back(std::move(c));
			} else if (key == "minlevel" || key == "maxlevel") {
				auto& c = cond(key == "minlevel" ? Kind::kMinLevel : Kind::kMaxLevel, val, v);
				if (!num(v, c.a) || c.a <= 0) conds.pop_back();   // 0: no limit
			} else if (key == "mindayspassed") {
				auto& c = cond(Kind::kMinDays, val, v);
				if (!num(v, c.f)) conds.pop_back();
			} else if (key == "afterletter") {
				for (auto& e : Split(val, ',')) {   // "SLOT" or "SLOT DAYS"
					auto&              c = cond(Kind::kAfterLetter, e, v);
					std::istringstream s(v);
					c.f = -1.f;
					if (!(s >> c.a) || c.a < 1 || c.a > kSlots) { bad("AfterLetter: a slot, 1 to " + std::to_string(kSlots) + ": " + e); conds.pop_back(); continue; }
					if (float d; s >> d) c.f = d;
				}
			} else if (key == "questcompleted" || key == "faction") {
				const bool quest = key == "questcompleted";
				for (auto& e : Split(val, ',')) {
					auto& c = cond(quest ? Kind::kQuestDone : Kind::kFaction, e, v);
					c.form = ParseForm(v);
					if (quest ? !(c.form && c.form->As<RE::TESQuest>()) : !(c.form && c.form->As<RE::TESFaction>())) {
						bad(quest ? "QuestCompleted: no quest " + v
								  : "Faction: no faction " + v + " (by editor ID only with powerofthree's Tweaks; Plugin.esp|0xID always works)");
						conds.pop_back();
					}
				}
			} else if (key == "queststage" || key == "global" || key == "hasitem") {
				const auto k = key == "queststage" ? Kind::kQuestStage : key == "global" ? Kind::kGlobal : Kind::kHasItem;
				for (auto& e : Split(val, ',')) {
					auto& c = cond(k, e, v);
					float n = 0;
					const bool ok = ParseFormNumber(v, k == Kind::kQuestStage ? 0.f : 1.f, c.form, n) &&
					                (k == Kind::kQuestStage ? c.form->As<RE::TESQuest>() != nullptr
									: k == Kind::kGlobal    ? c.form->As<RE::TESGlobal>() != nullptr
															: c.form->As<RE::TESBoundObject>() != nullptr);
					if (!ok) {
						bad(rawKey + (k == Kind::kQuestStage ? ": no quest in " : k == Kind::kGlobal ? ": no global in " : ": no item ") + v);
						conds.pop_back();
						continue;
					}
					if (k == Kind::kGlobal) c.f = n;
					else c.a = k == Kind::kHasItem ? std::max(1, static_cast<int>(n)) : static_cast<int>(n);
				}
			} else if (key == "hours") {
				auto&      c = cond(Kind::kHours, val, v);
				const auto parts = Split(v, '-');
				try {
					if (parts.size() != 2) throw std::invalid_argument("");
					c.a = std::stoi(parts[0]), c.b = std::stoi(parts[1]);
				} catch (...) {
					bad("Hours: like 18-6");
					conds.pop_back();
				}
			} else bad("unknown key: " + rawKey);
			if (!conds.empty()) scope().lines.push_back(std::move(conds));
		}
		while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) text.pop_back();
		L.text = text;
		if (L.slot < 1 || L.slot > kSlots) bad("Slot must be 1 to " + std::to_string(kSlots));
		for (std::size_t g = 0; g < L.any.size(); ++g)
			if (L.any[g].lines.empty()) bad("[Any] section " + std::to_string(g + 1) + " has no conditions; ignored");
		std::erase_if(L.any, [](const Group& g) { return g.lines.empty(); });
		auto own = [&](const Group& g) {
			for (auto& l : g.lines)
				for (auto& c : l)
					if (c.kind == Kind::kAfterLetter && !c.neg && c.a == L.slot) bad(c.text + ": its own slot, so it never comes");
		};
		own(L.all);
		for (auto& g : L.any) own(g);
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
			logger::info("letter {}: '{}' ({}){}, {} condition line(s), {} [Any] section(s), {} enclosure(s)", L.slot, L.title, L.file,
				L.enabled ? "" : " - disabled", L.all.lines.size(), L.any.size(), L.items.size());
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

	using InventoryCounts = decltype(std::declval<RE::PlayerCharacter&>().GetInventoryCounts());

	struct Context
	{
		RE::PlayerCharacter*           pc;
		RE::Calendar*                  cal;
		float                          now;
		const std::vector<float>&      delivered;
		float                          afterDays;   // the AfterLetterDays that applies
		std::optional<InventoryCounts> inventory;   // read once, when a HasItem asks
	};

	bool Test(const Cond& c, Context& x)
	{
		auto* pc = x.pc;
		if (c.kind == Kind::kRace || c.kind == Kind::kName) {
			std::string a, b;   // what the entries are compared with
			if (c.kind == Kind::kName) a = Lower(pc->GetName());
			else if (auto* race = pc->GetRace()) {
				a = Lower(race->GetName());
				b = race->GetFormEditorID() ? Lower(race->GetFormEditorID()) : "";
			}
			auto is = [&](const std::string& e) { return e == a || (!b.empty() && e == b); };
			return (c.any.empty() || std::ranges::any_of(c.any, is)) && std::ranges::none_of(c.none, is);
		}
		bool r = false;
		switch (c.kind) {
		case Kind::kSex: {
			auto* base = pc->GetActorBase();
			r = base && static_cast<int>(base->GetSex()) == c.a;
			break;
		}
		case Kind::kMinLevel: r = pc->GetLevel() >= c.a; break;
		case Kind::kMaxLevel: r = pc->GetLevel() <= c.a; break;
		case Kind::kMinDays: r = x.now >= c.f; break;
		case Kind::kAfterLetter: {
			const auto i = static_cast<std::size_t>(c.a - 1);
			const float days = c.f >= 0.f ? c.f : x.afterDays;
			r = i < x.delivered.size() && x.delivered[i] >= 0.f && x.now - x.delivered[i] >= days;
			break;
		}
		case Kind::kQuestDone: r = c.form->As<RE::TESQuest>()->IsCompleted(); break;
		case Kind::kQuestStage: {
			auto* q = c.form->As<RE::TESQuest>();
			r = q->IsCompleted() || q->GetCurrentStageID() >= c.a;
			break;
		}
		case Kind::kFaction: r = pc->IsInFaction(c.form->As<RE::TESFaction>()); break;
		case Kind::kGlobal: r = c.form->As<RE::TESGlobal>()->value >= c.f; break;
		case Kind::kHasItem: {
			if (!x.inventory) x.inventory = pc->GetInventoryCounts();
			const auto it = x.inventory->find(c.form->As<RE::TESBoundObject>());
			r = it != x.inventory->end() && it->second >= c.a;
			break;
		}
		case Kind::kHours: {
			const int h = static_cast<int>(x.cal->GetHour());
			r = c.a <= c.b ? (h >= c.a && h < c.b) : (h >= c.a || h < c.b);
			break;
		}
		default: break;
		}
		return r != c.neg;
	}

	bool Holds(const Letter& L, const std::vector<float>& delivered, std::string& why)
	{
		auto* pc = RE::PlayerCharacter::GetSingleton();
		auto* cal = RE::Calendar::GetSingleton();
		if (!pc || !cal) return why = "no player", false;
		if (!L.inCombat && pc->IsInCombat()) return why = "in combat", false;
		if (auto* cell = pc->GetParentCell(); cell && L.place != Place::kAnywhere) {
			if (L.place == Place::kOutdoors && cell->IsInteriorCell()) return why = "indoors", false;
			if (L.place == Place::kIndoors && !cell->IsInteriorCell()) return why = "outdoors", false;
		}
		const float letterDays = std::max(0.f, L.all.afterDays);
		Context     x{ pc, cal, cal->GetDaysPassed(), delivered, letterDays, std::nullopt };
		for (auto& line : L.all.lines)
			for (auto& c : line)
				if (!Test(c, x)) return why = c.text, false;
		for (std::size_t g = 0; g < L.any.size(); ++g) {
			const auto& group = L.any[g];
			x.afterDays = group.afterDays >= 0.f ? group.afterDays : letterDays;
			std::string failed;   // the first entry that fails, of each line
			bool        some = false;
			for (auto& line : group.lines) {
				const auto it = std::ranges::find_if(line, [&](const Cond& c) { return !Test(c, x); });
				if (it == line.end()) { some = true; break; }
				failed += (failed.empty() ? "" : "; ") + it->text;
			}
			if (!some) return why = "[Any] " + std::to_string(g + 1) + ": none of " + failed, false;
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

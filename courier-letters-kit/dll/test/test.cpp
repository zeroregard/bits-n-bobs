#include "Main.cpp"   // a copy, next to the stub PCH.h (run.sh)
using namespace RE;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { ++fails; std::cout << "FAIL line " << __LINE__ << ": " #c "\n"; } } while (0)

static Letter P(const std::string& name, const std::string& body) {
	auto p = std::filesystem::path(WORK) / name; std::ofstream(p) << body; return ParseFile(p);
}
static bool Due(const Letter& L, std::vector<float> d = std::vector<float>(30, -1.f), std::string* w = nullptr) {
	std::string why; bool r = Holds(L, d, why); if (w) *w = why; return r;
}

int main() {
	PlayerCharacter pc; Calendar cal; PlayerCharacter::single = &pc; Calendar::single = &cal;
	TESRace nord; nord.name = "Nord"; nord.edid = "NordRace";
	TESRace imp; imp.name = "Imperial"; imp.edid = "ImperialRace";
	TESQuest mq104; mq104.edid = "MQ104"; TESForm::byEdid["MQ104"] = &mq104;
	TESQuest mq105; mq105.edid = "MQ105"; TESForm::byEdid["MQ105"] = &mq105;
	TESFaction comp; TESDataHandler::forms[{ "Skyrim.esm", 0x048362 }] = &comp;
	TESGlobal glob; glob.edid = "G"; TESForm::byEdid["G"] = &glob;
	TESBoundObject ring; TESDataHandler::forms[{ "Skyrim.esm", 0x0877B1 }] = &ring;
	TESBoundObject gold; TESForm::byId[0xF] = &gold;
	pc.race = &nord; pc.level = 5;

	// backward compatibility: the shipped examples
	for (auto& e : std::filesystem::directory_iterator(EXAMPLES)) {
		auto L = ParseFile(e.path());
		for (auto& p : L.problems) std::cout << e.path().filename() << ": " << p << "\n";
		CHECK(L.problems.empty());
	}
	auto ex2 = ParseFile(EXAMPLES "/02_admirer.ini");
	pc.base.sex = SEX::kFemale; pc.level = 10; cal.hour = 19;
	CHECK(Due(ex2)); CHECK(ex2.items.size() == 2);
	pc.race = &imp; CHECK(Due(ex2));
	pc.race = &nord; pc.level = 9; CHECK(!Due(ex2)); pc.level = 10;
	cal.hour = 23; CHECK(!Due(ex2)); cal.hour = 19;
	pc.base.sex = SEX::kMale; CHECK(!Due(ex2)); pc.base.sex = SEX::kFemale;
	auto ex3 = ParseFile(EXAMPLES "/03_followup.ini");
	std::vector<float> d(30, -1.f); cal.days = 10;
	mq104.done = true; pc.inv[&ring] = 1;
	CHECK(!Due(ex3, d)); d[1] = 9; CHECK(!Due(ex3, d)); d[1] = 8; CHECK(Due(ex3, d));
	pc.inv.clear(); CHECK(!Due(ex3, d)); pc.inv[&ring] = 1; mq104.done = false; CHECK(!Due(ex3, d)); mq104.done = true;

	// example 4: level 5+, not a Companion, and MQ104 done or carrying the ring
	auto ex4 = ParseFile(EXAMPLES "/04_either_or.ini");
	pc.level = 5; pc.factions.clear(); mq104.done = false; pc.inv.clear(); CHECK(!Due(ex4));
	mq104.done = true; CHECK(Due(ex4)); mq104.done = false; pc.inv[&ring] = 1; CHECK(Due(ex4));
	pc.factions.insert(&comp); CHECK(!Due(ex4)); pc.factions.clear(); pc.level = 4; CHECK(!Due(ex4));
	pc.level = 10; mq104.done = true; pc.inv[&ring] = 1;

	// NOT
	auto n = P("n.ini", "Slot=5\nQuestCompleted = !MQ105\nFaction = !Skyrim.esm|0x048362\nRace = !Nord, !Argonian\n");
	CHECK(n.problems.empty());
	pc.race = &imp; CHECK(Due(n));
	mq105.done = true; CHECK(!Due(n)); mq105.done = false;
	pc.factions.insert(&comp); CHECK(!Due(n)); pc.factions.clear();
	pc.race = &nord; { std::string w; CHECK(!Due(n, d, &w)); CHECK(w == "Race = !Nord, !Argonian"); }
	pc.race = &imp;
	// plain + not race: one of the plain ones, none of the "!"
	auto r2 = P("r2.ini", "Slot=6\nRace = Nord, Imperial, !ImperialRace\n");
	pc.race = &nord; CHECK(Due(r2)); pc.race = &imp; CHECK(!Due(r2));
	// Race split over two lines outside [Any] stays one list (as before)
	auto r3 = P("r3.ini", "Slot=6\nRace = Nord\nRace = Imperial\n");
	CHECK(Due(r3)); pc.race = &nord; CHECK(Due(r3));
	// NOT on HasItem, Global, QuestStage, MinLevel, Hours, Sex, AfterLetter
	auto n2 = P("n2.ini", "Slot=7\nHasItem = !Skyrim.esm|0x0877B1\nGlobal = !G 2\nQuestStage = !MQ105 30\n");
	CHECK(n2.problems.empty());
	pc.inv.clear(); glob.value = 1; mq105.stage = 10; CHECK(Due(n2));
	pc.inv[&ring] = 1; CHECK(!Due(n2)); pc.inv.clear();
	glob.value = 2; CHECK(!Due(n2)); glob.value = 1;
	mq105.stage = 30; CHECK(!Due(n2)); mq105.stage = 10; mq105.done = true; CHECK(!Due(n2)); mq105.done = false;
	auto n3 = P("n3.ini", "Slot=8\nMinLevel = !20\nHours = !9-17\nSex = !Male\n");
	pc.level = 19; cal.hour = 20; pc.base.sex = SEX::kFemale; CHECK(Due(n3));
	pc.level = 20; CHECK(!Due(n3)); pc.level = 19;
	cal.hour = 12; CHECK(!Due(n3)); cal.hour = 17; CHECK(Due(n3));
	pc.base.sex = SEX::kMale; CHECK(!Due(n3)); pc.base.sex = SEX::kFemale;
	auto n4 = P("n4.ini", "Slot=9\nAfterLetter = !3\n");
	std::vector<float> d2(30, -1.f); CHECK(Due(n4, d2)); d2[2] = 1; CHECK(!Due(n4, d2));
	// AfterLetter inline days, and a list
	auto a1 = P("a1.ini", "Slot=10\nAfterLetter = 2 3, 4\nAfterLetterDays = 1\n");
	d2.assign(30, -1.f); cal.days = 10; d2[1] = 8; d2[3] = 9; CHECK(!Due(a1, d2)); d2[1] = 7; CHECK(Due(a1, d2)); d2[3] = 9.5f; CHECK(!Due(a1, d2));

	// [Any]
	auto any = P("any.ini", "[Letter]\nSlot=11\n[Conditions]\nMinLevel = 5\n[Any]\nQuestCompleted = MQ104, MQ105\nFaction = Skyrim.esm|0x048362\n[Any]\nRace = Nord\nSex = Female\n[Text]\nhi\n");
	CHECK(any.problems.empty()); CHECK(any.any.size() == 2); CHECK(any.all.lines.size() == 1); CHECK(any.text == "hi");
	pc.level = 5; pc.race = &imp; pc.base.sex = SEX::kFemale; mq104.done = true; mq105.done = false; pc.factions.clear();
	{ std::string w; CHECK(!Due(any, d, &w)); CHECK(w == "[Any] 1: none of QuestCompleted = MQ105; Faction = Skyrim.esm|0x048362"); std::cout << "why: " << w << "\n"; }
	mq105.done = true; CHECK(Due(any));                       // line 1 (both quests)
	mq105.done = false; pc.factions.insert(&comp); CHECK(Due(any));   // line 2
	pc.base.sex = SEX::kMale; CHECK(!Due(any)); pc.race = &nord; CHECK(Due(any));   // second [Any]
	pc.level = 4; CHECK(!Due(any)); pc.level = 5;
	// AfterLetterDays per [Any] section, falling back to the letter's
	auto ad = P("ad.ini", "Slot=12\nAfterLetterDays = 5\n[Any]\nAfterLetter = 1\nAfterLetter = 2\n[Any]\nAfterLetterDays = 1\nAfterLetter = 3\n");
	d2.assign(30, -1.f); cal.days = 10; d2[0] = 6; d2[2] = 9.5f; CHECK(!Due(ad, d2)); d2[0] = 5; CHECK(!Due(ad, d2)); d2[2] = 9; CHECK(Due(ad, d2));
	// [Any] with a NOT line: "not finished MQ104, or level 20+"
	auto an = P("an.ini", "Slot=13\n[Any]\nQuestCompleted = !MQ104\nMinLevel = 20\n");
	mq104.done = false; pc.level = 1; CHECK(Due(an)); mq104.done = true; CHECK(!Due(an)); pc.level = 20; CHECK(Due(an));

	// mistakes
	auto bad = P("bad.ini", "Slot=14\n[Any]\nTitle = x\nGold = 5\n[Any]\n[Conditions]\nAfterLetter = 14\nItem = !Skyrim.esm|0x0877B1\nAfterLetter = 40\nFaction = !Nope\n");
	for (auto& p : bad.problems) std::cout << "  problem: " << p << "\n";
	CHECK(bad.problems.size() == 8); CHECK(bad.any.empty()); CHECK(bad.items.size() == 2);
	std::cout << (fails ? "FAILED " : "all passed ") << fails << "\n";
	return fails != 0;
}

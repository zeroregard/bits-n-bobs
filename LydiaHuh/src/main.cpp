#include <spdlog/sinks/basic_file_sink.h>

namespace
{
	// Lydia's placed reference in Skyrim.esm
	constexpr RE::FormID kLydiaRef = 0x000A2C8E;

	struct Settings
	{
		std::uint32_t timerIntervalSeconds{ 600 };
		std::uint32_t timerChance{ 50 };
		std::uint32_t combatChance{ 5 };
		float         maxDistance{ 4000.0f };
		std::string   line{ "Huh?" };
		std::uint32_t notification{ 2 };
	};

	Settings          settings;
	std::uint32_t     elapsedSeconds{ 0 };  // main thread only
	std::atomic<bool> lydiaInCombat{ false };

	void InitializeLogging()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return;
		}
		*path /= "LydiaHuh.log";
		auto log = std::make_shared<spdlog::logger>(
			"Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");
	}

	void LoadSettings()
	{
		constexpr auto ini = "Data\\SKSE\\Plugins\\LydiaHuh.ini";
		constexpr auto sec = "General";

		const auto getInt = [&](const char* a_key, std::uint32_t a_default) {
			return static_cast<std::uint32_t>(GetPrivateProfileIntA(sec, a_key, static_cast<INT>(a_default), ini));
		};

		settings.timerIntervalSeconds = std::max(1u, getInt("TimerIntervalSeconds", settings.timerIntervalSeconds));
		settings.timerChance = std::min(100u, getInt("TimerChance", settings.timerChance));
		settings.combatChance = std::min(100u, getInt("CombatChance", settings.combatChance));
		settings.maxDistance = static_cast<float>(getInt("MaxDistance", 4000));
		settings.notification = getInt("Notification", settings.notification);

		char buf[256]{};
		GetPrivateProfileStringA(sec, "Line", settings.line.c_str(), buf, sizeof(buf), ini);
		settings.line = buf;

		logger::info("Settings: every {}s @ {}%, combat @ {}%, maxDistance {}, line '{}', notification {}",
			settings.timerIntervalSeconds, settings.timerChance, settings.combatChance,
			settings.maxDistance, settings.line, settings.notification);
	}

	bool Roll(std::uint32_t a_percent)
	{
		thread_local std::mt19937 rng{ std::random_device{}() };
		return std::uniform_int_distribution<std::uint32_t>{ 0, 99 }(rng) < a_percent;
	}

	// "Huh?" -> "huh", so punctuation and case don't matter when matching lines
	std::string Normalize(std::string_view a_text)
	{
		std::string out;
		for (const char c : a_text) {
			if (std::isalpha(static_cast<unsigned char>(c))) {
				out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
		}
		return out;
	}

	RE::Actor* GetLydiaIfNearby()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lydia = RE::TESForm::LookupByID<RE::Actor>(kLydiaRef);
		if (!player || !lydia || !player->GetParentCell() || !lydia->Get3D() ||
			lydia->IsDead() || lydia->IsDisabled()) {
			return nullptr;
		}
		if (lydia->GetDistance(player) > settings.maxDistance) {
			return nullptr;
		}
		return lydia;
	}

	// Vanilla generic dialogue lines (e.g. what she says when you bump into her) whose
	// text is the configured line and whose conditions pass for Lydia, i.e. lines
	// recorded in her own voice. Found by text at runtime, so no form IDs are hardcoded.
	std::vector<RE::TESTopicInfo*> FindLines(RE::Actor* a_lydia)
	{
		std::vector<RE::TESTopicInfo*> found;
		const auto  wanted = Normalize(settings.line);
		auto*       player = RE::PlayerCharacter::GetSingleton();
		auto*       data = RE::TESDataHandler::GetSingleton();
		if (!data || wanted.empty()) {
			return found;
		}

		for (auto* topic : data->GetFormArray<RE::TESTopic>()) {
			if (!topic || !topic->ownerQuest || !topic->topicInfos ||
				topic->data.subtype == RE::DIALOGUE_DATA::Subtype::kCustom ||
				topic->data.subtype == RE::DIALOGUE_DATA::Subtype::kScene) {
				continue;
			}
			for (std::uint32_t i = 0; i < topic->numTopicInfos; ++i) {
				auto* info = topic->topicInfos[i];
				if (!info || !info->objConditions(a_lydia, player)) {
					continue;
				}

				RE::BSTSmartPointer<RE::DialogueItem> item{
					new RE::DialogueItem(topic->ownerQuest, topic, info, a_lydia)
				};
				std::uint32_t count = 0;
				bool          matches = false;
				for (auto* response : item->responses) {
					++count;
					const char* text = response ? response->text.c_str() : nullptr;
					matches = text && Normalize(text) == wanted;
					if (matches) {
						logger::info("Found line {:08X} in topic '{}' ({:08X}): '{}' -> {}",
							info->GetFormID(), topic->GetFormEditorID(), topic->GetFormID(),
							text, response->voice.c_str());
					}
				}
				if (matches && count == 1) {
					found.push_back(info);
				}
			}
		}

		logger::info("{} matching line(s) for Lydia", found.size());
		return found;
	}

	// Papyrus' ObjectReference.Say() takes a whole topic and picks one of its lines.
	// To get exactly our line, the topic is narrowed to just that line until Say()
	// returns. Count and pointer are updated in an order that keeps any concurrent
	// reader within bounds.
	struct NarrowedTopic
	{
		std::mutex            lock;
		RE::TESTopic*         topic{ nullptr };
		RE::TESTopicInfo**    infos{ nullptr };
		std::uint32_t         count{ 0 };
		RE::TESTopicInfo*     single[1]{};
		std::chrono::steady_clock::time_point since;

		bool Narrow(RE::TESTopicInfo* a_info)
		{
			std::scoped_lock l{ lock };
			if (topic) {
				return false;
			}
			topic = a_info->parentTopic;
			infos = topic->topicInfos;
			count = topic->numTopicInfos;
			single[0] = a_info;
			since = std::chrono::steady_clock::now();
			topic->numTopicInfos = 1;
			topic->topicInfos = single;
			return true;
		}

		void Restore()
		{
			std::scoped_lock l{ lock };
			if (!topic) {
				return;
			}
			topic->topicInfos = infos;
			topic->numTopicInfos = count;
			topic = nullptr;
		}

		void RestoreIfStale()
		{
			bool stale;
			{
				std::scoped_lock l{ lock };
				stale = topic && std::chrono::steady_clock::now() - since > 5s;
			}
			if (stale) {
				logger::warn("Say() never returned; restoring topic");
				Restore();
			}
		}
	};

	NarrowedTopic narrowed;

	class RestoreOnReturn : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		void operator()(RE::BSScript::Variable) override { narrowed.Restore(); }
		void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
	};

	bool Say(RE::Actor* a_lydia, RE::TESTopicInfo* a_info)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* handles = vm ? vm->GetObjectHandlePolicy() : nullptr;
		auto* binds = vm ? vm->GetObjectBindPolicy() : nullptr;
		if (!handles || !binds) {
			return false;
		}

		const auto handle = handles->GetHandleForObject(RE::Actor::FORMTYPE, a_lydia);
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		if (!vm->FindBoundObject(handle, "Actor", object) && !vm->FindBoundObject(handle, "ObjectReference", object)) {
			if (!vm->CreateObject("Actor", object) || !object) {
				logger::warn("Could not create a script object for Lydia");
				return false;
			}
			binds->BindObject(object, handle);
		}

		if (!narrowed.Narrow(a_info)) {
			return false;  // previous line still in flight
		}

		auto* args = RE::MakeFunctionArguments(
			static_cast<RE::TESTopic*>(a_info->parentTopic), static_cast<RE::Actor*>(nullptr), false);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new RestoreOnReturn() };
		if (!vm->DispatchMethodCall(object, "Say", args, callback)) {
			logger::warn("Say() dispatch failed");
			narrowed.Restore();
			return false;
		}
		return true;
	}

	// Main thread only
	void SayHuh(const char* a_reason)
	{
		auto* lydia = GetLydiaIfNearby();
		if (!lydia) {
			return;
		}

		static std::vector<RE::TESTopicInfo*> lines;
		static std::uint32_t                  searches = 0;
		if (lines.empty() && searches < 3) {
			++searches;
			lines = FindLines(lydia);
		}

		logger::info("Huh? ({})", a_reason);

		bool said = false;
		if (!lines.empty()) {
			thread_local std::mt19937 rng{ std::random_device{}() };
			auto* info = lines[std::uniform_int_distribution<std::size_t>{ 0, lines.size() - 1 }(rng)];
			said = Say(lydia, info);
		}

		if (settings.notification == 1 || (settings.notification == 2 && !said)) {
			RE::SendHUDMessage::ShowHUDMessage(("Lydia: " + settings.line).c_str());
		}
	}

	// Main thread, once per second of wall time
	void Tick()
	{
		narrowed.RestoreIfStale();

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* ui = RE::UI::GetSingleton();
		if (!player || !player->GetParentCell() || !ui || ui->GameIsPaused()) {
			return;
		}
		if (++elapsedSeconds < settings.timerIntervalSeconds) {
			return;
		}
		elapsedSeconds = 0;
		if (Roll(settings.timerChance)) {
			SayHuh("timer");
		}
	}

	void StartTimer()
	{
		std::thread([] {
			for (;;) {
				std::this_thread::sleep_for(1s);
				if (auto* tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask(Tick);
				}
			}
		}).detach();
	}

	class CombatSink : public RE::BSTEventSink<RE::TESCombatEvent>
	{
	public:
		static CombatSink* GetSingleton()
		{
			static CombatSink singleton;
			return &singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*) override
		{
			if (!a_event || !a_event->actor || a_event->actor->GetFormID() != kLydiaRef) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// Only the "none -> fighting" edge counts as combat starting; going
			// between searching and fighting within one fight doesn't.
			const bool nowInCombat = a_event->newState != RE::ACTOR_COMBAT_STATE::kNone;
			const bool wasInCombat = lydiaInCombat.exchange(nowInCombat);
			if (nowInCombat && !wasInCombat && Roll(settings.combatChance)) {
				SKSE::GetTaskInterface()->AddTask([] { SayHuh("combat"); });
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(CombatSink::GetSingleton());
			StartTimer();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			elapsedSeconds = 0;
			lydiaInCombat = false;
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, false);
	InitializeLogging();
	LoadSettings();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	logger::info("LydiaHuh loaded");
	return true;
}

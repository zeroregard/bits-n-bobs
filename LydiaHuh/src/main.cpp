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
		std::string   soundPath{ "Sound\\FX\\LydiaHuh\\huh.wav" };
		float         volume{ 1.0f };
		std::uint32_t notification{ 2 };
	};

	Settings           settings;
	bool               soundExists{ false };
	std::uint32_t      elapsedSeconds{ 0 };  // main thread only
	RE::BSSoundHandle  lastHuh;              // main thread only
	std::atomic<bool>  lydiaInCombat{ false };

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
		settings.volume = static_cast<float>(std::min(100u, getInt("Volume", 100))) / 100.0f;
		settings.notification = getInt("Notification", settings.notification);

		char buf[MAX_PATH]{};
		GetPrivateProfileStringA(sec, "SoundPath", settings.soundPath.c_str(), buf, sizeof(buf), ini);
		settings.soundPath = buf;

		logger::info("Settings: every {}s @ {}%, combat @ {}%, maxDistance {}, sound '{}', volume {}, notification {}",
			settings.timerIntervalSeconds, settings.timerChance, settings.combatChance,
			settings.maxDistance, settings.soundPath, settings.volume, settings.notification);
	}

	bool Roll(std::uint32_t a_percent)
	{
		thread_local std::mt19937 rng{ std::random_device{}() };
		return std::uniform_int_distribution<std::uint32_t>{ 0, 99 }(rng) < a_percent;
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

	// Main thread only
	void SayHuh(const char* a_reason)
	{
		auto* lydia = GetLydiaIfNearby();
		if (!lydia) {
			return;
		}
		if (lastHuh.IsValid() && lastHuh.IsPlaying()) {
			return;
		}

		logger::info("Huh? ({})", a_reason);

		if (settings.notification == 1 || (settings.notification == 2 && !soundExists)) {
			RE::SendHUDMessage::ShowHUDMessage("Lydia: Huh?");
		}
		if (!soundExists) {
			return;
		}

		auto* audio = RE::BSAudioManager::GetSingleton();
		if (!audio) {
			return;
		}

		RE::BSSoundHandle  handle;
		RE::BSResource::ID id;
		id.GenerateFromPath(settings.soundPath.c_str());
		audio->GetSoundHandleByFile(handle, id, 0x1A, 128);
		if (!handle.IsValid()) {
			logger::warn("Could not build sound from '{}'", settings.soundPath);
			return;
		}

		handle.SetVolume(settings.volume);
		auto* root = lydia->Get3D();
		auto* head = root ? root->GetObjectByName(RE::BSFixedString{ "NPC Head [Head]" }) : nullptr;
		if (auto* node = head ? head : root) {
			handle.SetObjectToFollow(node);
		}
		handle.Play();
		lastHuh = handle;
	}

	// Main thread, once per second of wall time
	void Tick()
	{
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
			soundExists = RE::BSResourceNiBinaryStream{ settings.soundPath }.good();
			if (!soundExists) {
				logger::warn("Sound '{}' not found; Lydia will only say 'Huh?' as a notification", settings.soundPath);
			}
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

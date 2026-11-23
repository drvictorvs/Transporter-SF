#include "CountdownMenu.h"

namespace CountdownMenu
{
	namespace
	{
		using Value = RE::Scaleform::GFx::Value;
		using Clock = std::chrono::steady_clock;

		constexpr std::string_view kMenuName = "Transporter_SF_Countdown";

		// DefaultGravJumpAnim in GravJumpComponent.swf: 99 frames; the countdown text reaches
		// 00:00 at frame 91 (its DEFAULT_END_FRAME) and the rest is the closing animation.
		constexpr double kFrames = 99.0;
		constexpr double kCountdownFrames = 91.0;

		// Its frame scripts play these at the frames listed; the countdown ticks every second.
		struct StageSound
		{
			double      frame;
			const char* id;
		};
		constexpr std::array kStageSounds{
			StageSound{ 11.0, "UICockpitHUDAGravJumpSequenceBLoad" },
			StageSound{ 31.0, "UICockpitHUDAGravJumpSequenceCMod" },
			StageSound{ 52.0, "UICockpitHUDAGravJumpSequenceDSpool" },
			StageSound{ 74.0, "UICockpitHUDAGravJumpSequenceECalc" },
			StageSound{ 87.0, "UICockpitHUDAGravJumpSequenceYReady" },
		};
		constexpr auto kTickSound = "UICockpitHUDAGravJumpSequenceXSecond";

		// Where the ship HUD places the box on its 1920x1080 stage (SpaceshipHudMenu.swf).
		constexpr double kBoxX = 960.0;
		constexpr double kBoxY = 572.6;

		constexpr double kHoldAfterEnd = 0.3;  // seconds the finished box stays before closing
		constexpr std::uintptr_t kTick = 1;

		// Set by Show(), read by the menu on the UI's side.
		std::atomic<float>    g_seconds{ 3.0f };
		std::atomic<unsigned> g_requests{ 0 };

		double Total(double a_seconds)
		{
			return a_seconds * kFrames / kCountdownFrames;
		}

		class Menu final : public RE::GameMenuBase
		{
		public:
			SF_MENU_NAME(kMenuName);

			Menu()
			{
				menuName = MENU_NAME.data();
				// No flags: it doesn't pause the game, take input or show the cursor.
			}

			const char*   GetName() const override { return MENU_NAME.data(); }
			const char*   GetRootPath() const override { return "root1"; }
			ScaleModeType GetViewScaleMode() override { return ScaleModeType::kShowAll; }

			RE::BSEventNotifyControl ProcessEvent(const RE::UpdateSceneRectEvent&, RE::BSTEventSource<RE::UpdateSceneRectEvent>*) override
			{
				return RE::BSEventNotifyControl::kContinue;
			}

			void PostCreate() override
			{
				depthPriority = 0x0A;  // below the Pause menu (0x0B), above the HUD
			}

			bool ShouldHandleEvent(const RE::InputEvent*) override { return false; }

			bool LoadMovie(bool a_addEventDispatcher, bool a_arg2) override
			{
				const bool loaded = GameMenuBase::LoadMovie(a_addEventDispatcher, a_arg2);
				if (loaded) {
					Start();
				} else {
					REX::WARN("Countdown: {}.swf did not load", kMenuName);
				}
				return loaded;
			}

			void Call(const Params& a_params) noexcept override
			{
				if (reinterpret_cast<std::uintptr_t>(a_params.userData) == kTick) {
					Tick();
				}
			}

		private:
			void Start()
			{
				auto* root = uiMovie ? uiMovie->asMovieRoot.get() : nullptr;
				if (!root) {
					return Close("no movie root");
				}
				if (!menuObj.IsObject()) {
					root->GetVariable(&menuObj, "root");
				}
				if (!menuObj.IsDisplayObject()) {
					return Close("no stage");
				}

				root->CreateObject(&m_box, "DefaultGravJumpComponent");
				if (!m_box.IsDisplayObject()) {
					return Close("DefaultGravJumpComponent not found: is GravJumpComponent.swf installed?");
				}
				m_box.SetMember("x", Value(kBoxX));
				m_box.SetMember("y", Value(kBoxY));
				menuObj.Invoke("addChild", nullptr, &m_box, 1);

				// The box is driven once per frame from the movie's own enterFrame event.
				root->CreateFunction(&m_tick, this, reinterpret_cast<void*>(kTick));
				const Value args[2]{ Value("enterFrame"), m_tick };
				menuObj.Invoke("addEventListener", nullptr, args, 2);

				REX::INFO("Countdown: box created");
				Begin();
				Tick();
			}

			void Begin()
			{
				m_request = g_requests.load();
				m_total = Total(g_seconds.load());
				m_start = Clock::now();
				m_nextStage = 0;
				m_lastSecond = -1;
				m_closing = false;
			}

			void Tick()
			{
				if (g_requests.load() != m_request) {
					if (m_closing) {
						// Shown again after this one asked to close: open a fresh one once it has.
						m_request = g_requests.load();
						if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
							queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kShow);
						}
						return;
					}
					Begin();  // shown again while still up: start over
				}
				if (m_closing || !m_box.IsDisplayObject()) {
					return;
				}

				const double elapsed = std::chrono::duration<double>(Clock::now() - m_start).count();
				// Below 1 so the box's own arithmetic, which divides by (1 - progress), stays finite.
				const double progress = std::clamp(elapsed / m_total, 0.0, 0.999);
				const double left = std::max(m_total - elapsed, 0.001);

				Value data;
				uiMovie->asMovieRoot->CreateObject(&data);
				data.SetMember("gravJumpCalculatedPercentage", Value(progress));
				data.SetMember("fGravJump_SecsUntilJump", Value(left));
				data.SetMember("bGravJumpAnimStarted", Value(true));
				data.SetMember("bGravJumpInitiated", Value(true));  // hides the overhaul's Abort Jump button
				m_box.Invoke("RefreshGravJumpComponent", nullptr, &data, 1);

				PlaySounds(progress, left);

				if (elapsed >= m_total + kHoldAfterEnd) {
					Close(nullptr);
				}
			}

			void PlaySounds(double a_progress, double a_left)
			{
				const double frame = 1.0 + (kFrames - 1.0) * a_progress;
				while (m_nextStage < kStageSounds.size() && frame >= kStageSounds[m_nextStage].frame) {
					RE::PlayMenuSound(kStageSounds[m_nextStage].id);
					++m_nextStage;
				}
				// The same seconds the box shows: the time left minus its closing frames.
				const double shown = std::max(a_left - m_total * (kFrames - kCountdownFrames) / kFrames, 0.0);
				const auto   second = static_cast<int>(shown);
				if (second != m_lastSecond) {
					if (m_lastSecond >= 0) {
						RE::PlayMenuSound(kTickSound);
					}
					m_lastSecond = second;
				}
			}

			void Close(const char* a_problem)
			{
				if (a_problem) {
					REX::WARN("Countdown: {}", a_problem);
				}
				m_closing = true;
				if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
					queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kHide);
				}
			}

			Value             m_box;
			Value             m_tick;
			Clock::time_point m_start{};
			double            m_total{ 3.0 };
			std::size_t       m_nextStage{ 0 };
			int               m_lastSecond{ -1 };
			unsigned          m_request{ 0 };
			bool              m_closing{ false };
		};

		RE::Scaleform::Ptr<RE::IMenu> Create()
		{
			return RE::Scaleform::Ptr<RE::IMenu>{ new Menu() };
		}
	}

	bool Register()
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return false;
		}
		const RE::BSFixedString name(kMenuName.data());
		if (!ui->IsMenuRegistered(name)) {
			ui->RegisterMenu(kMenuName.data(), &Create, true);
		}
		const bool registered = ui->IsMenuRegistered(name);
		if (registered) {
			REX::INFO("Countdown: menu {} registered", kMenuName);
		} else {
			REX::WARN("Countdown: could not register menu {}", kMenuName);
		}
		return registered;
	}

	bool IsRegistered()
	{
		const auto ui = RE::UI::GetSingleton();
		return ui && ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()));
	}

	float TotalSeconds(float a_seconds)
	{
		return static_cast<float>(Total(a_seconds));
	}

	const char* Show(float a_seconds)
	{
		const auto ui = RE::UI::GetSingleton();
		const auto queue = RE::UIMessageQueue::GetSingleton();
		const RE::BSFixedString name(kMenuName.data());
		const char* problem = !ui ? "the game's UI isn't available" :
		                      !queue ? "the game's UI message queue isn't available" :
		                      !ui->IsMenuRegistered(name) ? "the countdown menu isn't registered with the game's UI" :
		                                                    nullptr;
		if (problem) {
			REX::WARN("Countdown: not shown: {}", problem);
			return problem;
		}
		g_seconds = std::clamp(a_seconds, 0.5f, 99.0f);
		++g_requests;
		const bool open = ui->IsMenuOpen(name);
		if (!open) {
			queue->AddMessage(name, RE::UI_MESSAGE_TYPE::kShow);
		}
		REX::INFO("Countdown: {} for {:.1f} s", open ? "restarting" : "opening", g_seconds.load());
		return nullptr;
	}
}

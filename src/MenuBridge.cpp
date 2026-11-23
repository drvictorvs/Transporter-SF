#include "MenuBridge.h"

#include "CountdownMenu.h"
#include "OSFSettings.h"
#include "OSFUI.h"
#include "Papyrus.h"

namespace MenuBridge
{
	namespace
	{
		using VM = RE::BSScript::IVirtualMachine;
		using Request = OSFUI::API::Request;

		constexpr auto kNativeScript = "Transporter_SFNative";
		constexpr auto kTargetsEndpoint = "transporter-sf.targets";  // the page's "targets", in its mod's namespace
		constexpr auto kBeamEndpoint = "transporter-sf.beam";
		constexpr auto kView = "transporter-sf/menu";

		// The BindEverythingToScript call in GameVM's constructor: the same place OSF Settings
		// and OSF UI hook to bind their natives. Hooks there chain, so all of them run.
		constexpr std::ptrdiff_t kBindCallOffset = 0x802;

		OSFUI::API::Client                          g_ui;
		OSFSettings::API::Client                    g_settings;
		std::atomic_bool                            g_hotkey{ false };  // the transporter hotkey is registered
		std::mutex                                  g_lock;
		std::unordered_map<std::uint64_t, Request>  g_pending;  // "targets" requests waiting for the script
		std::optional<REL::THook<void(VM**)>>       g_bindHook;

		void OnGameThread(std::function<void()> a_task)
		{
			if (const auto tasks = SFSE::GetTaskInterface()) {
				tasks->AddTask(std::move(a_task));
			} else {
				a_task();
			}
		}

		std::optional<Request> TakePending(std::uint64_t a_token)
		{
			std::lock_guard lock(g_lock);
			const auto it = g_pending.find(a_token);
			if (it == g_pending.end()) {
				return std::nullopt;
			}
			Request request = it->second;
			g_pending.erase(it);
			return request;
		}

		// A JSON string literal; the text is UTF-8 and passes through unchanged apart from escapes.
		void AppendQuoted(std::string& a_out, std::string_view a_text)
		{
			a_out += '"';
			for (const char c : a_text) {
				switch (c) {
				case '"':  a_out += "\\\""; break;
				case '\\': a_out += "\\\\"; break;
				case '\n': a_out += "\\n"; break;
				case '\r': a_out += "\\r"; break;
				case '\t': a_out += "\\t"; break;
				default:
					if (static_cast<unsigned char>(c) < 0x20) {
						a_out += std::format("\\u{:04x}", static_cast<unsigned>(static_cast<unsigned char>(c)));
					} else {
						a_out += c;
					}
				}
			}
			a_out += '"';
		}

		// The first string in {"args":["...", ...]}, the payload of osfui.send(name, value).
		std::string FirstArg(std::string_view a_json)
		{
			auto pos = a_json.find("\"args\"");
			pos = pos == std::string_view::npos ? pos : a_json.find('[', pos);
			pos = pos == std::string_view::npos ? pos : a_json.find_first_not_of(" \t\r\n", pos + 1);
			if (pos == std::string_view::npos || a_json[pos] != '"') {
				return {};
			}
			std::string value;
			for (++pos; pos < a_json.size(); ++pos) {
				const char c = a_json[pos];
				if (c == '"') {
					return value;
				}
				if (c == '\\' && pos + 1 < a_json.size()) {
					const char next = a_json[++pos];
					value += next == 'n' ? '\n' : next == 't' ? '\t' : next;
				} else {
					value += c;
				}
			}
			return {};
		}

		// ---- Transporter_SFNative's functions -------------------------------------------------

		bool IsAvailable(VM&, std::uint32_t, std::monostate)
		{
			return static_cast<bool>(g_ui);
		}

		bool OpenView(VM&, std::uint32_t, std::monostate, RE::BSFixedString a_viewId)
		{
			return a_viewId.c_str() && g_ui.RequestMenu(a_viewId.c_str(), true);
		}

		bool CloseView(VM&, std::uint32_t, std::monostate, RE::BSFixedString a_viewId)
		{
			return a_viewId.c_str() && g_ui.RequestMenu(a_viewId.c_str(), false);
		}

		bool ReplyTargets(VM&, std::uint32_t, std::monostate, RE::BSFixedString a_token, std::vector<RE::BSFixedString> a_lines)
		{
			const auto token = std::strtoull(a_token.c_str() ? a_token.c_str() : "", nullptr, 10);
			const auto request = TakePending(token);
			if (!request) {
				return false;  // already answered, timed out, or not ours
			}
			std::string json = "[";
			for (std::size_t i = 0; i < a_lines.size(); ++i) {
				if (i) {
					json += ',';
				}
				AppendQuoted(json, a_lines[i].c_str() ? a_lines[i].c_str() : "");
			}
			json += ']';
			request->Reply(json.c_str());
			return true;
		}

		// Shows the grav-jump countdown box. Returns "" when shown, otherwise why not.
		// (Debug.ExecuteConsole can't reach the TransporterCountdown console command.)
		// The menu itself is opened from the game's main thread (scripts run elsewhere); a problem
		// found there is written to Transporter_SF.log ("Countdown: not shown: ...").
		RE::BSFixedString ShowCountdown(VM&, std::uint32_t, std::monostate, float a_seconds)
		{
			if (!CountdownMenu::IsRegistered()) {
				REX::WARN("Countdown: not shown: the countdown menu isn't registered with the game's UI");
				return RE::BSFixedString("the countdown menu isn't registered");
			}
			const float seconds = a_seconds > 0.0f ? a_seconds : 3.0f;
			OnGameThread([seconds]() { CountdownMenu::Show(seconds); });
			return RE::BSFixedString("");
		}

		bool IsUnder(RE::BGSLocation* a_location, const RE::BGSLocation* a_ancestor)
		{
			for (int depth = 0; a_location && depth < 32; ++depth) {
				if (a_location == a_ancestor) {
					return true;
				}
				a_location = a_location->parentLocation.get();
			}
			return false;
		}

		// Map-marker references kept in the worldspaces' persistent cells (where map markers
		// live, loaded or not), whose location lies under akPlanetLocation. With no planet
		// location, the markers of akAnchor's worldspace. Names, discovery and distance are
		// left to the script.
		std::vector<RE::TESObjectREFR*> FindMapMarkers(VM& a_vm, std::uint32_t a_stack, std::monostate, RE::BGSLocation* a_planet, RE::TESObjectREFR* a_anchor)
		{
			std::vector<RE::TESObjectREFR*> found;
			const auto data = RE::TESDataHandler::GetSingleton();
			if (!data) {
				REX::WARN("FindMapMarkers: no data handler");
				return found;
			}
			RE::TESWorldSpace* anchorWorld = a_anchor ? a_anchor->GetParentWorldSpace() : nullptr;
			if (!a_planet && !anchorWorld) {
				REX::INFO("FindMapMarkers: no planet and no worldspace (interior?)");
				return found;
			}
			std::size_t worlds = 0, cells = 0, refs = 0, markers = 0, noLocation = 0;
			auto& worldArray = data->formArrays[std::to_underlying(RE::FormType::kWRLD)];
			const RE::BSAutoReadLock lock(worldArray.lock);
			for (const auto& form : worldArray.formArray) {
				const auto world = form ? form->As<RE::TESWorldSpace>() : nullptr;
				if (!world) {
					continue;
				}
				++worlds;
				if (!a_planet && world != anchorWorld) {
					continue;
				}
				const auto cell = world->unk0D0.get();
				if (!cell || !cell->Is(RE::FormType::kCELL)) {
					continue;
				}
				++cells;
				cell->ForEachReference([&](const RE::NiPointer<RE::TESObjectREFR>& a_ref) {
					++refs;
					if (!a_ref->extraDataList || !a_ref->extraDataList->HasType(RE::ExtraDataType::kMapMarker)) {
						return RE::BSContainer::ForEachResult::kContinue;
					}
					++markers;
					if (a_planet) {
						const auto location = a_ref->GetCurrentLocation();
						if (!location) {
							++noLocation;
							return RE::BSContainer::ForEachResult::kContinue;
						}
						if (!IsUnder(location, a_planet)) {
							return RE::BSContainer::ForEachResult::kContinue;
						}
					}
					found.push_back(a_ref.get());
					return found.size() >= 1000 ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
				});
			}
			REX::INFO("FindMapMarkers: {} worldspaces, {} persistent cells, {} refs, {} map markers ({} without a location), {} kept ({})",
				worlds, cells, refs, markers, noLocation, found.size(), a_planet ? "on the planet" : "in this worldspace");
			if (found.empty() && a_planet && anchorWorld) {
				// Nothing matched the planet's location (or it was the wrong one): fall back to
				// the markers of the anchor's own worldspace.
				return FindMapMarkers(a_vm, a_stack, {}, nullptr, a_anchor);
			}
			return found;
		}

		// The DLL's version, so the script can tell when the two come from different builds.
		RE::BSFixedString DllVersion(VM&, std::uint32_t, std::monostate)
		{
			const auto version = SFSE::GetPluginVersion();
			return RE::BSFixedString(std::format("{}.{}.{}", version.major(), version.minor(), version.patch()).c_str());
		}

		bool HotkeyRegistered(VM&, std::uint32_t, std::monostate)
		{
			return g_hotkey.load();
		}

		void BindNatives(VM& a_vm)
		{
			a_vm.BindNativeMethod(kNativeScript, "IsAvailable", &IsAvailable, false, false);
			a_vm.BindNativeMethod(kNativeScript, "OpenView", &OpenView, false, false);
			a_vm.BindNativeMethod(kNativeScript, "CloseView", &CloseView, false, false);
			a_vm.BindNativeMethod(kNativeScript, "ReplyTargets", &ReplyTargets, false, false);
			a_vm.BindNativeMethod(kNativeScript, "ShowCountdown", &ShowCountdown, false, false);
			a_vm.BindNativeMethod(kNativeScript, "FindMapMarkers", &FindMapMarkers, false, false);
			a_vm.BindNativeMethod(kNativeScript, "DllVersion", &DllVersion, false, false);
			a_vm.BindNativeMethod(kNativeScript, "HotkeyRegistered", &HotkeyRegistered, false, false);
			REX::INFO("MenuBridge: script functions bound on {}", kNativeScript);
		}

		void OnVMBind(VM** a_vm)
		{
			(*g_bindHook)(a_vm);
			if (a_vm && *a_vm) {
				BindNatives(**a_vm);  // every time: the game may build the VM more than once
			}
		}

		// ---- OSF UI endpoints ------------------------------------------------------------

		void OnTargets(const Request& a_request, void*) noexcept
		{
			try {
				{
					std::lock_guard lock(g_lock);
					g_pending.insert_or_assign(a_request.m_token, a_request);
				}
				const auto token = a_request.m_token;
				OnGameThread([token]() {
					if (!Papyrus::Call("AnswerTargets", std::to_string(token))) {
						if (const auto request = TakePending(token)) {
							request->Reject("unavailable", "the Papyrus VM isn't ready");
						}
					}
				});
			} catch (...) {
				a_request.Reject("internal", "Transporter_SF could not queue the request");
			}
		}

		void OnBeam(const char*, const char* a_payloadJson, const char*, void*) noexcept
		{
			try {
				std::string target = FirstArg(a_payloadJson ? a_payloadJson : "");
				if (target.empty()) {
					REX::WARN("MenuBridge: beam without a target: {}", a_payloadJson ? a_payloadJson : "");
					return;
				}
				g_ui.RequestMenu(kView, false);
				OnGameThread([target = std::move(target)]() {
					if (!Papyrus::Call("BeamFromMenu", target)) {
						REX::WARN("MenuBridge: BeamFromMenu not queued: Papyrus VM unavailable");
					}
				});
			} catch (...) {
				REX::WARN("MenuBridge: beam failed");
			}
		}
	}

	bool InstallBindHook()
	{
		g_bindHook.emplace("Transporter_SF::BindNatives", RE::ID::GameVM::Ctor, kBindCallOffset, &OnVMBind);
		if (!g_bindHook->Init()) {
			REX::ERROR("MenuBridge: native-binding hook could not be set up; binding after data load instead");
			g_bindHook.reset();
			return false;
		}
		g_bindHook->Enable();
		REX::INFO("MenuBridge: script functions will bind whenever the game builds its Papyrus VM");
		return true;
	}

	namespace
	{
		// Each time OSF UI shows the menu, tell the page so it resets and scans again.
		void OnViewLifecycle(const char* a_viewId, OSFUI::API::ViewLifecyclePhase a_phase, void*) noexcept
		{
			if (a_phase == OSFUI::API::ViewLifecyclePhase::kShown && a_viewId) {
				g_ui.EmitEvent(a_viewId, "opened", "{}");
			}
		}
	}

	namespace
	{
		constexpr auto kModId = "transporter-sf";
		constexpr auto kHotkeyId = "openTransporter";

		// Runs inline during input handling: only queue the menu to open. The page itself
		// shows why the transporter can't be used (combat...) and keeps Energize disabled.
		void OnHotkey(const char*, const char*, void*) noexcept
		{
			if (!g_ui || !g_ui.RequestMenu(kView, true)) {
				REX::WARN("MenuBridge: hotkey pressed, but OSF UI couldn't open {}", kView);
			}
		}
	}

	void RegisterHotkey()
	{
		if (g_hotkey.load()) {
			return;
		}
		if (!g_settings && !g_settings.Init()) {
			REX::WARN("MenuBridge: OSF Settings' C++ interface isn't available; no transporter hotkey");
			return;
		}
		// Registered once for the whole run (OSF Settings keeps C++ hotkeys across loads), so
		// nothing has to call into Papyrus while a save is loading.
		const auto status = g_settings.RegisterHotkey(kModId, kHotkeyId, &OnHotkey, nullptr);
		if (status == OSFSettings::API::Status::Ok || status == OSFSettings::API::Status::AlreadyRegistered) {
			g_hotkey = status == OSFSettings::API::Status::Ok;
		}
		REX::INFO("MenuBridge: transporter hotkey registration: status {}", static_cast<std::uint32_t>(status));
	}

	void Connect()
	{
		if (!g_ui.Init()) {
			REX::WARN("MenuBridge: OSF UI 2.x's C++ interface isn't available; the transporter menu can't work");
			return;
		}
		const bool targets = g_ui.RegisterRequest(kTargetsEndpoint, &OnTargets, nullptr);
		const bool beam = g_ui.RegisterSend(kBeamEndpoint, &OnBeam, nullptr);
		const bool lifecycle = g_ui.RegisterViewLifecycle(kView, &OnViewLifecycle, nullptr);
		REX::INFO("MenuBridge: connected to OSF UI (interface {:x}); targets {}, beam {}, reopen events {}", g_ui.Version(), targets, beam, lifecycle);
	}

	void BindIfUnhooked()
	{
		if (g_bindHook) {
			return;
		}
		if (const auto game = RE::GameVM::GetSingleton(); game && game->GetVM()) {
			BindNatives(*game->GetVM());
		}
	}
}

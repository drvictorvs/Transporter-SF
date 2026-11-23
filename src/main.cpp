// Transporter_SF: a Star Trek-style transporter for Starfield (split from CCFE-SF).
//
// The DLL connects the transporter menu and its hotkey to OSF UI and OSF Settings
// (MenuBridge.cpp), shows the grav-jump countdown box (CountdownMenu.cpp), and registers three
// console commands with ModernCommandFramework. Teleport and TransporterMenu are handed, as one
// string of space-separated arguments, to the global function of the same name in
// Transporter_SF.psc, so they also work without this DLL:  cgf "Transporter_SF.Teleport" "0001ABCD"
// Nothing here calls into Papyrus while a save loads.

#include "CountdownMenu.h"
#include "MCF.h"
#include "MenuBridge.h"
#include "Papyrus.h"
#include "TravelProbe.h"

namespace
{
	constexpr auto kScript = "Transporter_SF";

	struct Command
	{
		const char* name;      // what you type in the console
		const char* function;  // global function in Transporter_SF.psc
	};

	// To add a command: write `Function Name(String asArgs = "") global` in Transporter_SF.psc
	// and add a line here.
	constexpr std::array kCommands{
		Command{ "Teleport", "Teleport" },
		Command{ "TransporterMenu", "TransporterMenu" },
	};

	// MCF hands over the arguments already split (and with quotes removed); the
	// Papyrus side splits on spaces again, so they are joined back with single spaces.
	std::string JoinArgs(const MCF::simple_array<MCF::simple_string_view>& a_args)
	{
		std::string joined;
		for (std::uint64_t i = 0; i < a_args.size(); ++i) {
			const std::string_view arg = a_args[i];
			if (!joined.empty()) {
				joined += ' ';
			}
			joined += arg;
		}
		return joined;
	}

	template <std::size_t I>
	void OnCommand(const MCF::simple_array<MCF::simple_string_view>& a_args, const char*, MCF::ConsoleInterface* a_console)
	{
		const auto& command = kCommands[I];
		if (!Papyrus::Call(command.function, JoinArgs(a_args))) {
			const auto message = std::format("{}: Papyrus is not ready; you can still type cgf \"{}.{}\" \"<arguments>\"", command.name, kScript, command.function);
			a_console->PrintLn(message);
		}
	}

	// TransporterCountdown [seconds]: shows the grav-jump countdown box, for testing from the console.
	// The script uses Transporter_SFNative.ShowCountdown instead: Debug.ExecuteConsole can't reach this.
	void OnCountdownCommand(const MCF::simple_array<MCF::simple_string_view>& a_args, const char*, MCF::ConsoleInterface* a_console)
	{
		float seconds = 3.0f;
		if (a_args.size() > 0) {
			const std::string_view arg = a_args[0];
			const std::string      text{ arg };
			char*                  end = nullptr;
			const float            parsed = std::strtof(text.c_str(), &end);
			if (end != text.c_str() && parsed > 0.0f) {
				seconds = parsed;
			}
		}
		if (const char* problem = CountdownMenu::Show(seconds); problem && a_console) {
			a_console->PrintLn(std::format("TransporterCountdown: {}", problem).c_str());
		}
	}

	void Register(const char* a_name, MCF::CommandCallback a_callback)
	{
		if (MCF::RegisterCommand(a_name, a_callback)) {
			REX::INFO("registered {}", a_name);
		} else {
			REX::WARN("could not register {}: ModernCommandFramework.dll is not loaded", a_name);
		}
	}

	template <std::size_t... I>
	void RegisterAll(std::index_sequence<I...>)
	{
		(Register(kCommands[I].name, &OnCommand<I>), ...);
	}

	void OnMessage(SFSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}
		switch (a_message->type) {
		case SFSE::MessagingInterface::kPostLoad:
			// every SFSE plugin is loaded, so ModernCommandFramework.dll can be found
			RegisterAll(std::make_index_sequence<kCommands.size()>{});
			Register("TransporterCountdown", &OnCountdownCommand);
			MenuBridge::RegisterHotkey();
			break;
		case SFSE::MessagingInterface::kPostPostLoad:
			// OSF UI hands out its C++ interface from here on
			MenuBridge::Connect();
			MenuBridge::RegisterHotkey();  // no-op once registered
			break;
		case SFSE::MessagingInterface::kPostDataLoad:
			MenuBridge::BindIfUnhooked();
			MenuBridge::RegisterHotkey();  // no-op once registered
			CountdownMenu::Register();
			break;
		default:
			break;
		}
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	// The trampoline holds the jump for MenuBridge's hook; hooks are installed by hand, as
	// OSF Settings and OSF UI do theirs.
	SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 64, .hook = false });
	MenuBridge::InstallBindHook();
	TravelProbe::Install();
	SFSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}

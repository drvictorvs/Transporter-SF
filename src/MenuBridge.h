#pragma once

// The transporter menu's connection to OSF UI, made from C++.
//
// OSF UI 2.0.0 binds its Papyrus functions (OSFUI.Open, OSFUI.RegisterRequestStatic...) only
// once, the first time the game builds its Papyrus VM. The game builds it a second time (in
// your game, when a save starts loading; OSF Settings' log shows its own functions being bound
// twice), and from then on every OSFUI.* call from a script fails with "Unbound native function".
//
// So the script no longer calls OSF UI. This module:
//   - registers the page's endpoints with OSF UI's C++ interface (they last for the whole run):
//       "targets" (request): asks Transporter_SF.AnswerTargets(token) for the list, which answers
//                            through Transporter_SFNative.ReplyTargets
//       "beam"    (send):    closes the menu and calls Transporter_SF.BeamFromMenu(target)
//   - gives the script its own functions on Transporter_SFNative (IsAvailable, OpenView, CloseView,
//     ReplyTargets), bound every time the game builds the VM, the way OSF Settings does.
namespace MenuBridge
{
	// At plugin load: hooks the VM's native-binding call (needs the trampoline).
	bool InstallBindHook();

	// At kPostPostLoad: acquires OSF UI's C++ interface and registers the endpoints.
	void Connect();

	// At kPostLoad (again later if it failed): registers the "Open the transporter" hotkey with
	// OSF Settings' C++ interface; pressing it opens the menu.
	void RegisterHotkey();

	// Binds Transporter_SFNative's functions on the current VM now. Only needed when the hook failed;
	// called at kPostDataLoad.
	void BindIfUnhooked();
}

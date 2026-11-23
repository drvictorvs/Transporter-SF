#pragma once

// First step toward delaying fast travel: find where the game confirms it.
//
// With [TravelHook] iMode=1 in Transporter_SF.ini, this hooks the two callback types the game's own
// type names tie to confirming a fast travel, logs every call to Transporter_SF.log (arguments and the
// first bytes of the callback object), and then lets the call run exactly as before.
// With iMode=0 (the default) it does nothing.
//
// The logs answer what the delaying hook needs to know: which callback fires when you confirm a
// fast travel, which vtable slot is its "run" function, and how the object is laid out.
namespace TravelProbe
{
	void Install();
}

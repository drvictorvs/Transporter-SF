#pragma once

namespace Papyrus
{
	// Queues Transporter_SF.<function>(asArgs) on the Papyrus VM; false when the VM isn't up yet.
	// The call runs later, on the VM's own schedule.
	bool Call(const char* a_function, std::string a_args);
}

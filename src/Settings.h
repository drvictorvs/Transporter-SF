#pragma once

// Minimal reader for Data\SFSE\Plugins\Transporter_SF.ini (the same file the Papyrus side reads).
namespace Settings
{
	// The integer value of a_key in [a_section], or a_default when the file, section or key is
	// missing or the value isn't a number. Section and key names ignore case.
	int GetInt(std::string_view a_section, std::string_view a_key, int a_default);
}

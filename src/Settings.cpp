#include "Settings.h"

namespace
{
	constexpr auto kPath = "Data\\SFSE\\Plugins\\Transporter_SF.ini";

	std::string Trim(std::string_view a_text)
	{
		const auto first = a_text.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		const auto last = a_text.find_last_not_of(" \t\r\n");
		return std::string(a_text.substr(first, last - first + 1));
	}

	bool SameText(std::string_view a_left, std::string_view a_right)
	{
		return a_left.size() == a_right.size() &&
		       std::equal(a_left.begin(), a_left.end(), a_right.begin(), [](char a, char b) {
				   return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			   });
	}
}

namespace Settings
{
	int GetInt(std::string_view a_section, std::string_view a_key, int a_default)
	{
		std::ifstream file(kPath);
		if (!file) {
			return a_default;
		}

		std::string line;
		bool        inSection = false;
		while (std::getline(file, line)) {
			const auto text = Trim(line);
			if (text.empty() || text.front() == ';' || text.front() == '#') {
				continue;
			}
			if (text.front() == '[') {
				const auto close = text.find(']');
				inSection = close != std::string::npos && SameText(Trim(std::string_view(text).substr(1, close - 1)), a_section);
				continue;
			}
			if (!inSection) {
				continue;
			}
			const auto equals = text.find('=');
			if (equals == std::string::npos || !SameText(Trim(std::string_view(text).substr(0, equals)), a_key)) {
				continue;
			}
			const auto value = Trim(std::string_view(text).substr(equals + 1));
			int        result = a_default;
			const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
			return error == std::errc{} ? result : a_default;
		}
		return a_default;
	}
}

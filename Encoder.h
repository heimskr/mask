#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Chemion {
	std::vector<uint8_t> encode(const std::array<char, 168> &);
	std::vector<uint8_t> encode(std::string_view);

	std::vector<uint8_t> fromColumns(const std::array<std::array<bool, 7>, 24> &);
}

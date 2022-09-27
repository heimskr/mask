#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace Chemion {
	std::vector<uint8_t> encode(const std::array<char, 168> &);
	std::vector<uint8_t> encode(std::string_view);
	std::vector<uint8_t> fromColumns(const std::span<std::array<bool, 7>> &);
	std::vector<uint8_t> encodeString(std::string_view);
}

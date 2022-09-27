#include <span>
#include <stdexcept>

#include "Encoder.h"
#include "Font.h"

namespace Chemion {
	static uint8_t getPair(char character) {
		if (character == ' ')
			return 0b00;
		if (character == '-')
			return 0b01;
		if (character == 'x')
			return 0b10;
		if (character == 'X')
			return 0b11;
		throw std::invalid_argument("Invalid character: " + std::to_string(static_cast<int>(character)));
	}

	std::vector<uint8_t> encode(const std::array<char, 168> &chars) {
		std::vector<uint8_t> out {0xfa, 0x03, 0x00, 0x39, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		uint8_t crc = 7;
		uint8_t byte = 0;
		uint8_t pair_count = 0;

		for (const char character: chars) {
			byte |= getPair(character) << (2 * (3 - pair_count));
			if (++pair_count == 4) {
				out.push_back(byte);
				pair_count = 0;
				crc ^= byte;
				byte = 0;
			}
		}

		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(crc);
		out.push_back(0x55);
		out.push_back(0xa9);

		return out;
	}

	std::vector<uint8_t> encode(std::string_view string) {
		std::array<char, 168> chars {};

		size_t index = 0;
		size_t line_index = 0;
		const size_t max = chars.size();

		for (const char character: string)
			if (character == '\n') {
				while (line_index < 24) {
					if (index == max)
						throw std::runtime_error("Too many characters");
					chars[index++] = ' ';
					++line_index;
				}
				line_index = 0;
			} else if (line_index == 24) {
				throw std::invalid_argument("Line too long");
			} else {
				if (index == max)
					throw std::runtime_error("Too many characters");
				chars[index++] = character;
				++line_index;
			}

		return encode(chars);
	}

	std::vector<uint8_t> fromColumns(const std::span<std::array<bool, 7>> &columns) {
		std::array<char, 168> flat {};
		std::fill(flat.begin(), flat.end(), ' ');
		for (size_t row = 0; row < 7; ++row)
			for (size_t column = 0; column < 24 && column < columns.size(); ++column)
				flat[row * 24 + column] = columns[column][row]? 'X' : ' ';
		return encode(flat);
	}

	std::vector<uint8_t> encodeString(std::string_view str) {
		std::vector<std::array<bool, 7>> columns;
		for (const char ch: str) {
			const auto &pixels = font.at(ch);
			for (int col = 0; col < 4; ++col) {
				std::array<bool, 7> column;
				for (int row = 0; row < 7; ++row) {
					column[row] = pixels[row][col];
				}
				columns.push_back(column);
			}
		}

		return fromColumns(columns);
	}
}

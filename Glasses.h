#pragma once

#include "Bluetooth.h"

namespace Chemion {
	class Image;
	struct Scroller;

	class Glasses {
		private:
			Bluetooth bluetooth;
			Characteristic *rx = nullptr;

		public:
			using Columns = std::vector<std::array<bool, 7>>;

			void setup(uint16_t index);
			bool connect(const char *addr);

			bool scroll(Scroller &, size_t initial_delay = 0, size_t count = -1);
			bool showString(std::string_view);
			bool display(const Image &);
	};
}

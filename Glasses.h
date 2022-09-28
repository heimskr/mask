#pragma once

#include "Bluetooth.h"

namespace Chemion {
	struct Scroller;

	class Glasses {
		private:
			Bluetooth bluetooth;
			Characteristic *rx = nullptr;

		public:
			bool connect(const char *addr);

			bool scroll(Scroller &, size_t initial_delay = 0, size_t count = -1);
			bool showString(std::string_view);
	};
}

#pragma once

#include <array>
#include <vector>

namespace Chemion {
	class Image {
		public:
			constexpr static int WIDTH = 24;
			constexpr static int HEIGHT = 7;

			std::vector<std::array<bool, 7>> data;

			void filledSquare(int x, int y, int w, int h);
			void squareOutline(int x, int y, int w, int h);
	};
}

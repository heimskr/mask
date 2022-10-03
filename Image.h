#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace Chemion {
	class Image {
		public:
			using Data = std::vector<std::array<bool, 7>>;

			constexpr static int WIDTH = 24;
			constexpr static int HEIGHT = 7;

			Data data;

			Image();
			Image(Data &&data_): data(data_) {}

			bool & operator()(size_t x, size_t y);

			void filledRectangle(int x, int y, int w, int h);
			void rectangleOutline(int x, int y, int w, int h);
			void circleOutline(int center_x, int center_y, int radius);
			void clear();
	};
}

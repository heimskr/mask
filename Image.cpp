#include <stdexcept>

#include "Image.h"

namespace Chemion {
	Image::Image() {
		data.resize(24);
	}

	bool & Image::operator()(size_t x, size_t y) {
		return data.at(x).at(y);
	}

	void Image::filledRectangle(int x, int y, int w, int h) {
		for (int col = x; col < x + w; ++col)
			for (int row = y; row < y + h; ++row)
				(*this)(col, row) = true;
	}

	void Image::rectangleOutline(int x, int y, int w, int h) {
		if (w == 1 && h == 1) {
			(*this)(x, y) = true;
			return;
		}

		for (int i = x; i < x + w; ++i) {
			(*this)(i, y) = true;
			(*this)(i, y + h - 1) = true;
		}

		for (int j = y + 1; j < y + h - 1; ++j) {
			(*this)(x, j) = true;
			(*this)(x + w - 1, j) = true;
		}
	}

	static void drawOnCircle(Image &image, int center_x, int center_y, int x, int y) {
		for (const auto [a, b]: std::initializer_list<std::pair<int, int>> {
			{x, y}, {x, -y}, {-x, y}, {-x, -y}, {y, x}, {y, -x}, {-y, x}, {-y, -x}
		}) {
			try {
				image(center_x + a, center_y + b) = true;
			} catch (const std::out_of_range &) {}
		}
	}

	void Image::circleOutline(int center_x, int center_y, int radius) {
		if (radius == 0) {
			(*this)(center_x, center_y) = true;
			return;
		}

		int x = 0;
		int y = radius;
		int d = 3 - 2 * radius;
		drawOnCircle(*this, center_x, center_y, x, y);
		while (x++ <= y) {
			if (0 < d) {
				--y;
				d += 4 * (x - y) + 10;
			} else
				d += 4 * x + 6;
			drawOnCircle(*this, center_x, center_y, x, y);
		}
	}

	void Image::clear() {
		for (auto &array: data)
			array.fill(false);
	}
}

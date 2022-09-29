#include "Image.h"

namespace Chemion {
	void Image::filledSquare(int x, int y, int w, int h) {
		for (int col = x; col < x + w; ++col)
			for (int row = y; row < y + h; ++row)
				data[col][row] = true;
	}

	void Image::squareOutline(int x, int y, int w, int h) {
		if (w == 1 && h == 1) {
			data[x][y] = true;
			return;
		}

		for (int i = x; i < x + w; ++i) {
			data[i][y] = true;
			data[i][y + h - 1] = true;
		}

		for (int j = y + 1; j < y + h - 1; ++j) {
			data[x][j] = true;
			data[x + w - 1][j] = true;
		}
	}
}

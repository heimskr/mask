#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <vector>
#include <thread>
#include <vector>

#include "Encoder.h"

namespace Chemion {
	struct Scroller {
		std::vector<std::array<bool, 7UL>> columns;
		ssize_t offset = 0;
		bool increasing = true;
		std::chrono::milliseconds edgeDelay;
		std::chrono::milliseconds delay;

		Scroller(std::string_view str, int64_t edge_delay = 800, int64_t delay_ = 200):
			columns(Chemion::stringColumns(str)), edgeDelay(edge_delay), delay(delay_) {}

		bool render(const std::function<bool(const std::vector<uint8_t> &, size_t)> &fn) {
			if (!fn(Chemion::fromColumns(std::span(columns).subspan(offset, 24)), 20))
				return false;

			if (increasing) {
				if (std::ssize(columns) - 24 <= offset) {
					increasing = false;
					std::this_thread::sleep_for(edgeDelay);
				} else
					++offset;
			} else {
				if (offset == 0) {
					increasing = true;
					std::this_thread::sleep_for(edgeDelay);
				} else
					--offset;
			}

			std::this_thread::sleep_for(delay);
			return true;
		}
	};
}

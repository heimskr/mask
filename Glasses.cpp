#include <cassert>

#include "Debug.h"
#include "Glasses.h"
#include "Scroller.h"

namespace Chemion {
	bool Glasses::connect(const char *addr) {
		if (!bluetooth.connectDevice(addr))
			return false;
		
		if (!bluetooth.waitForConnection()) {
			DBG("Failed to connect (%d, %d)", bluetooth.connected.load(), static_cast<int>(bluetooth.mgmt.state));
			return false;
		}

		if (!bluetooth.primary("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")) {
			DBG("Primary failed.");
			return false;
		}

		if (!bluetooth.waitForServices()) {
			DBG("Couldn't find services.");
			return false;
		}

		assert(bluetooth.services.size() == 1);

		if (!bluetooth.findCharacteristics()) {
			DBG("Couldn't get characteristics.");
			return false;
		}

		rx = bluetooth.findCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

		if (rx == nullptr) {
			DBG("Couldn't find RX characteristic.");
			for (const auto &charac: *bluetooth.characteristics)
				DBG("  %u, %u, %u, \"%s\"", charac.handle, charac.properties, charac.valueHandle, charac.uuid.c_str());
			return false;
		}

		return true;
	}

	bool Glasses::scroll(Scroller &scroller, size_t initial_delay, size_t count) {
		assert(rx != nullptr);

		auto batch = [this](const std::vector<uint8_t> &enc, size_t count) {
			return bluetooth.batch(enc, *rx, count);
		};

		for (size_t i = 0; i < count; ++i) {
			if (!scroller.render(batch))
				return false;
			if (i == 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(initial_delay));
		}

		return true;
	}

	bool Glasses::showString(std::string_view string) {
		if (rx == nullptr)
			return false;
		return bluetooth.batch(Chemion::encodeString(string), *rx, 20);
	}
}

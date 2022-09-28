// Contains code from bluepy.

#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <glib.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#include "lib/bluetooth.h"
#include "lib/sdp.h"
#include "lib/uuid.h"
#include "lib/mgmt.h"
#include "src/shared/mgmt.h"

#include "btio/btio.h"
#include "attrib/att.h"
#include "attrib/gattrib.h"
#include "attrib/gatt.h"
#include "attrib/gatttool.h"
}

#include "Bluetooth.h"
#include "Debug.h"
#include "Encoder.h"
#include "Timer.h"

static GMainLoop *event_loop = nullptr;

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

int main() {

	event_loop = g_main_loop_new(nullptr, false);
	bool running = true;

	Bluetooth bt;
	bt.setup(0);

	std::thread th([&] {
		if (!bt.connectDevice("EB:9F:86:0E:E2:F4"))
			return;

		if (!bt.waitForConnection()) {
			DBG("Failed to connect. Farewell, world! (%d, %d)", bt.connected.load(), static_cast<int>(bt.mgmt.state));
			return;
		}

		DBG("Connected.");

		if (!bt.primary("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")) {
			DBG("Primary failed.");
			return;
		}

		DBG("Primary succeeded.");

		if (!bt.waitForServices()) {
			DBG("Couldn't find services.");
			return;
		}

		DBG("Found services.");
		assert(bt.services.size() == 1);

		if (!bt.findCharacteristics()) {
			DBG("Couldn't get characteristics.");
			return;
		}

		auto *rxptr = bt.findCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

		if (rxptr == nullptr) {
			DBG("Couldn't find RX characteristic.");
			for (const auto &charac: *bt.characteristics)
				DBG("  %u, %u, %u, \"%s\"", charac.handle, charac.properties, charac.valueHandle, charac.uuid.c_str());
			return;
		}

		const Characteristic &rx = *rxptr;

		DBG("Found RX characteristic.");
		DBG("Connection complete.");

		// const char *image1 =
		// 	" XXX X  X XXX  XXXX XXX \n"
		// 	"X    X  X X  X X    X  X\n"
		// 	"X    X  X X  X X    X  X\n"
		// 	"X    XXXX XXX  XXX  XXX \n"
		// 	"X     XX  X  X X    X  X\n"
		// 	"X     XX  X  X X    X  X\n"
		// 	" XXX  XX  XXX  XXXX X  X";
		// const char *image2 =
		// 	" XX          XX      XX \n"
		// 	"XX X        XX X    X  X\n"
		// 	"XX X XX   X XX X    X  X\n"
		// 	"XX X XX X X XX X      X \n"
		// 	"XX X XX X X XX X      X \n"
		// 	"XX X XX X X XX X        \n"
		// 	" XX   XX X   XX       X ";
		// const char *image1 =
		// 	"X X X X X X X X X X X X \n"
		// 	" X X X X X X X X X X X X\n"
		// 	"X X X X X X X X X X X X \n"
		// 	" X X X X X X X X X X X X\n"
		// 	"X X X X X X X X X X X X \n"
		// 	" X X X X X X X X X X X X\n"
		// 	"X X X X X X X X X X X X ";
		// const char *image2 =
		// 	" X X X X X X X X X X X X\n"
		// 	"X X X X X X X X X X X X \n"
		// 	" X X X X X X X X X X X X\n"
		// 	"X X X X X X X X X X X X \n"
		// 	" X X X X X X X X X X X X\n"
		// 	"X X X X X X X X X X X X \n"
		// 	" X X X X X X X X X X X X";
		const char *image1 =
			"XXXXXXXXXXXX            \n"
			"XXXXXXXXXXXX            \n"
			"XXXXXXXXXXXX            \n"
			"XXXXXXXXXXXX            \n"
			"XXXXXXXXXXXX            \n"
			"XXXXXXXXXXXX            \n"
			"XXXXXXXXXXXX            ";
		const char *image2 =
			"            XXXXXXXXXXXX\n"
			"            XXXXXXXXXXXX\n"
			"            XXXXXXXXXXXX\n"
			"            XXXXXXXXXXXX\n"
			"            XXXXXXXXXXXX\n"
			"            XXXXXXXXXXXX\n"
			"            XXXXXXXXXXXX";

		// const auto enc1 = Chemion::encode(image1);
		// const auto enc2 = Chemion::encode(image2);

		std::vector<std::array<bool, 7>> cols1;
		std::vector<std::array<bool, 7>> cols2;
		for (size_t i = 0; i < 24 / 2; ++i) {
			cols1.push_back(std::array<bool, 7> {false, false, false, false, false, false, false});
			cols1.push_back(std::array<bool, 7> {true, true, true, true, true, true, true});
			cols2.push_back(std::array<bool, 7> {true, true, true, true, true, true, true});
			cols2.push_back(std::array<bool, 7> {false, false, false, false, false, false, false});
		}

		// const auto enc1 = Chemion::fromColumns(std::span(cols1));
		// const auto enc2 = Chemion::fromColumns(std::span(cols2));
		const auto enc1 = Chemion::encodeString("Hello,");
		const auto enc2 = Chemion::encodeString("World!");
		const auto enc3 = Chemion::encodeString("Hello, World!");

		DBG("Entering loop.");

		Scroller scroller("Has anyone really been far even as decided to use even go want to do look more like?", 3000, 40);

		auto batch = [&](const std::vector<uint8_t> &enc, size_t count) {
			return bt.batch(enc, rx, count);
		};

		for (size_t i = 0; i < 10000; ++i) {
			if (!scroller.render(batch))
				return;
			if (i == 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(1'000));
		}

		DBG("Writing succeeded.");
	});

	DBG("Starting loop");
	g_main_loop_run(event_loop);
	DBG("Exiting loop");

	running = false;
	th.join();
}

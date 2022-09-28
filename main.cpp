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

#include "Bluetooth.h"
#include "Debug.h"
#include "Glasses.h"
#include "Encoder.h"
#include "Scroller.h"
#include "Timer.h"

static GMainLoop *event_loop = nullptr;

int main() {

	event_loop = g_main_loop_new(nullptr, false);
	bool running = true;

	Bluetooth bt;
	bt.setup(0);

	std::thread th([&] {
		Chemion::Glasses glasses;

		if (!glasses.connect("EB:9F:86:0E:E2:F4")) {
			DBG("Couldn't connect to glasses.");
			return;
		}

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

		Chemion::Scroller scroller("Has anyone really been far even as decided to use even go want to do look more like?", 3000, 40);
		glasses.scroll(scroller, 1000);

		DBG("Done.");
	});

	DBG("Starting loop");
	g_main_loop_run(event_loop);
	DBG("Exiting loop");

	running = false;
	th.join();
}

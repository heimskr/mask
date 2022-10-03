// Contains code from bluepy.

#include "Bluetooth.h"
#include "Debug.h"
#include "Encoder.h"
#include "Glasses.h"
#include "Image.h"
#include "Scroller.h"
#include "Timer.h"

int main() {
	GMainLoop *event_loop = g_main_loop_new(nullptr, false);
	bool running = true;

	std::thread th([&] {
		auto wait = [](size_t millis) { std::this_thread::sleep_for(std::chrono::milliseconds(millis)); };

		Chemion::Glasses glasses;

		glasses.setup(0);

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

		glasses.showString("Hello,");
		wait(2'000);
		glasses.showString("World!");
		wait(2'000);

		Chemion::Image image;
		image.rectangleOutline(0, 0, 24, 7);
		image.filledRectangle(2, 2, 20, 3);
		glasses.display(image);
		wait(4'000);

		image.clear();
		image.circleOutline(3, 3, 2);
		image.circleOutline(16, 3, 3);
		image.circleOutline(16, 3, 1);
		glasses.display(image);
		wait(4'000);

		Chemion::Scroller scroller("Has anyone really been far even as decided to use even go want to do look more like?", 3'000, 40);
		glasses.scroll(scroller, 1'000);

		DBG("Done.");
	});

	DBG("Starting loop");
	g_main_loop_run(event_loop);
	DBG("Exiting loop");

	running = false;
	th.join();
}

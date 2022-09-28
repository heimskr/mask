#pragma once

#include <cstdint>

extern "C" {
#include "lib/bluetooth.h"
#include "lib/mgmt.h"
#include "src/shared/mgmt.h"
}

class Mgmt {
	private:
		mgmt *cobj = nullptr;
		uint16_t index = MGMT_INDEX_NONE;
		bool scan(bool starting);

	public:
		enum class State {Disconnected, Connecting, Connected, Scanning};

		State state = State::Disconnected;

		Mgmt();

		void setup(uint16_t new_index);
		bool startScan();
		bool stopScan();
};

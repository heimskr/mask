#pragma once

#include <atomic>
#include <cstdint>
#include <glib.h>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "Debug.h"
#include "CVPair.h"
#include "Mgmt.h"
#include "attrib/gattrib.h"

struct Service {
	uint16_t start;
	uint16_t end;
	Service(uint16_t start_, uint16_t end_): start(start_), end(end_) {}
};

struct Characteristic {
	uint16_t handle;
	uint8_t properties;
	uint16_t valueHandle;
	std::string uuid;

	Characteristic(uint16_t handle_, uint8_t properties_, uint16_t value_handle, std::string uuid_):
		handle(handle_), properties(properties_), valueHandle(value_handle), uuid(std::move(uuid_)) {}
};

struct CharacteristicEntry {
	CVPair pair;
	std::vector<Characteristic> characteristics;
	CharacteristicEntry() = default;
};

template <typename C>
static std::string toHex(const C &bytes) {
	std::ostringstream ss;
	for (const uint8_t byte: bytes)
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
	return ss.str();
}

class Bluetooth {
	public:
		// I can either make all these public or add a bunch of friend method declarations. Neither option is great.
		Mgmt mgmt;
		GIOChannel *iochannel = nullptr;
		GAttrib *attrib = nullptr;
		int opt_mtu = 0;
		CVPair cvConnect;
		std::atomic_bool connected {false};
		CVPair cvServices;
		std::vector<Service> services;
		std::atomic_bool services_ready {false};
		std::atomic_size_t nextCharacteristic {0};
		std::map<size_t, CharacteristicEntry> characteristicMap;
		std::optional<std::vector<Characteristic>> characteristics;
		CharacteristicEntry *nextEntry = nullptr;

		Bluetooth() = default;

		void setup(uint16_t index);
		bool connectDevice(const char *addr, const char *type = "random");
		void setDisconnected();
		void setConnected();
		void disconnectIO();
		bool primary(const char *uuid);
		Characteristic * findCharacteristic(std::string_view uuid);
		bool waitForConnection(size_t milliseconds = 5'000);
		bool waitForServices(size_t milliseconds = 5'000);
		bool findCharacteristics(uint16_t start = 1, uint16_t end = 0xffff, const char *uuid = nullptr);

		template <typename C>
		bool writeBytes(const Characteristic &characteristic, const C &bytes) {
			uint8_t *value = nullptr;
			size_t plen;
			int handle = characteristic.valueHandle;

			if (mgmt.state != Mgmt::State::Connected) {
				DBG("writeBytes: bad state");
				return false;
			}

			if (handle <= 0) {
				DBG("writeBytes: invalid handle");
				return false;
			}

			const std::string hex = toHex(bytes);
			plen = gatt_attr_data_from_string(hex.c_str(), &value);
			if (plen == 0) {
				DBG("writeBytes: plen == 0");
				return false;
			}

			gatt_write_cmd(attrib, handle, value, plen, nullptr, nullptr);
			g_free(value);
			return true;
		}

		bool writeByte(const Characteristic &, uint8_t);
};

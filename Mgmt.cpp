#include <cassert>

#include "Debug.h"
#include "Mgmt.h"

static void mgmt_device_connected(uint16_t index, uint16_t length, const void *param, void *user_data) {
	DBG("New device connected");
}

static void read_version_complete(uint8_t status, uint16_t length, const void *param, void *user_data) {
	const mgmt_rp_read_version *rp = (mgmt_rp_read_version *) param;

	if (status != MGMT_STATUS_SUCCESS) {
		DBG("Failed to read version information: %s (0x%02x)", mgmt_errstr(status), status);
		return;
	}

	if (length < sizeof(*rp)) {
		DBG("Wrong size of read version response");
		return;
	}

	DBG("Bluetooth management interface %u.%u initialized", rp->version, btohs(rp->revision));
}

static void mgmt_scanning(uint16_t index, uint16_t length, const void *param, void *user_data) {
	const mgmt_ev_discovering *ev = (mgmt_ev_discovering *) param;
	assert(length == sizeof(*ev));
	reinterpret_cast<Mgmt *>(user_data)->state = ev->discovering? Mgmt::State::Scanning : Mgmt::State::Disconnected;
}

static void mgmt_device_found(uint16_t index, uint16_t length, const void *param, void *user_data) {
	const mgmt_ev_device_found *ev = (mgmt_ev_device_found *) param;
	const uint8_t *val = ev->addr.bdaddr.b;
	assert(length == sizeof(*ev) + ev->eir_len);
	DBG("Device found: %02X:%02X:%02X:%02X:%02X:%02X type=%X flags=%X", val[5], val[4], val[3], val[2], val[1], val[0], ev->addr.type, ev->flags);
}

static void scan_cb(uint8_t status, uint16_t length, const void *param, void *user_data) {
	if (status != MGMT_STATUS_SUCCESS)
		DBG("Scan error: %s (%d)", mgmt_errstr(status), status);
}

Mgmt::Mgmt(): cobj(mgmt_new_default()) {}

void Mgmt::setup(uint16_t new_index) {
	if (cobj == nullptr) {
		DBG("Could not connect to the BT management interface, try with su rights");
		return;
	}

	DBG("Setting up mgmt on hci %u", new_index);
	index = new_index;

	mgmt_set_debug(cobj, +[](const char *str, void *user_data) {
		// std::cerr << str << reinterpret_cast<const char *>(user_data) << '\n';
	}, (void *) "mgmt: ", nullptr);

	if (mgmt_send(cobj, MGMT_OP_READ_VERSION, MGMT_INDEX_NONE, 0, nullptr, read_version_complete, this, nullptr) == 0)
		DBG("mgmt_send(MGMT_OP_READ_VERSION) failed");

	if (!mgmt_register(cobj, MGMT_EV_DEVICE_CONNECTED, index, mgmt_device_connected, this, nullptr))
		DBG("mgmt_register(MGMT_EV_DEVICE_CONNECTED) failed");

	if (!mgmt_register(cobj, MGMT_EV_DISCOVERING, index, mgmt_scanning, this, nullptr))
		DBG("mgmt_register(MGMT_EV_DISCOVERING) failed");

	if (!mgmt_register(cobj, MGMT_EV_DEVICE_FOUND, index, mgmt_device_found, this, nullptr))
		DBG("mgmt_register(MGMT_EV_DEVICE_FOUND) failed");
}

bool Mgmt::scan(bool starting) {
	mgmt_cp_start_discovery cp {(1 << BDADDR_LE_PUBLIC) | (1 << BDADDR_LE_RANDOM)};
	const uint16_t opcode = starting? MGMT_OP_START_DISCOVERY : MGMT_OP_STOP_DISCOVERY;

	if (cobj == nullptr)
		return false;

	if (mgmt_send(cobj, opcode, index, sizeof(cp), &cp, scan_cb, nullptr, nullptr) == 0) {
		DBG("mgmt_send(MGMT_OP_%s_DISCOVERY) failed", starting? "START" : "STOP");
		return false;
	}

	return true;
}

bool Mgmt::startScan() {
	return scan(true);
}

bool Mgmt::stopScan() {
	return scan(false);
}

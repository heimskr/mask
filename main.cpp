// Contains code from bluepy.

#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
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

#include "Encoder.h"
#include "Timer.h"

#define DBG(...) do { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)

static uint16_t mgmt_ind = MGMT_INDEX_NONE;
static mgmt *mgmt_master = nullptr;
static int opt_mtu = 0;
static GAttrib *attrib = nullptr;
static GIOChannel *iochannel = nullptr;

static enum state {
	STATE_DISCONNECTED = 0,
	STATE_CONNECTING = 1,
	STATE_CONNECTED = 2,
	STATE_SCANNING = 3,
} conn_state;

struct CVPair {
	std::mutex mutex;
	std::condition_variable var;

	template <typename... Args>
	void wait(Args &&...args) {
		std::unique_lock lock(mutex);
		var.wait(lock, args...);
	}

	template <typename D>
	bool wait_for(const D &duration) {
		std::unique_lock lock(mutex);
		return var.wait_for(lock, duration) == std::cv_status::no_timeout;
	}

	template <typename D, typename P>
	bool wait_for(const D &duration, const P &predicate) {
		std::unique_lock lock(mutex);
		return var.wait_for(lock, duration, predicate);
	}

	template <typename D>
	bool wait_until(const D &duration) {
		std::unique_lock lock(mutex);
		return var.wait_until(lock, duration) == std::cv_status::no_timeout;
	}

	template <typename D, typename P>
	bool wait_until(const D &duration, const P &predicate) {
		std::unique_lock lock(mutex);
		return var.wait_until(lock, duration, predicate);
	}

	void notify() {
		var.notify_all();
	}
};

CVPair connect_pair;
std::atomic_bool connected {false};

struct Service {
	uint16_t start;
	uint16_t end;
	Service(uint16_t start_, uint16_t end_): start(start_), end(end_) {}
};

CVPair services_pair;
std::vector<Service> services;
std::atomic_bool services_ready {false};
int commpipe[2];

static void scan_cb(uint8_t status, uint16_t length, const void *param, void *user_data) {
	if (status != MGMT_STATUS_SUCCESS) {
		DBG("Scan error: %s (%d)", mgmt_errstr(status), status);
		return;
	}

	// std::cerr << "scan_cb success\n";
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

static void mgmt_device_connected(uint16_t index, uint16_t length, const void *param, void *user_data) {
	DBG("New device connected");
}

static void set_state(enum state st) {
	conn_state = st;
}

static void mgmt_scanning(uint16_t index, uint16_t length, const void *param, void *user_data) {
	const mgmt_ev_discovering *ev = (mgmt_ev_discovering *) param;
	assert(length == sizeof(*ev));
	set_state(ev->discovering? STATE_SCANNING : STATE_DISCONNECTED);
}

static void mgmt_device_found(uint16_t index, uint16_t length, const void *param, void *user_data) {
	const mgmt_ev_device_found *ev = (mgmt_ev_device_found *) param;
	const uint8_t *val = ev->addr.bdaddr.b;
	assert(length == sizeof(*ev) + ev->eir_len);
	DBG("Device found: %02X:%02X:%02X:%02X:%02X:%02X type=%X flags=%X", val[5], val[4], val[3], val[2], val[1], val[0], ev->addr.type, ev->flags);
}

static void mgmt_setup(unsigned int idx) {
	mgmt_master = mgmt_new_default();
	if (!mgmt_master) {
		std::cerr << "Could not connect to the BT management interface, try with su rights\n";
		return;
	}

	std::cerr << "Setting up mgmt on hci" << idx << '\n';
	mgmt_ind = idx;
	mgmt_set_debug(mgmt_master, +[](const char *str, void *user_data) {
		// std::cerr << str << reinterpret_cast<const char *>(user_data) << '\n';
	}, (void *) "mgmt: ", nullptr);

	if (mgmt_send(mgmt_master, MGMT_OP_READ_VERSION, MGMT_INDEX_NONE, 0, NULL, read_version_complete, NULL, nullptr) == 0)
		std::cerr << "mgmt_send(MGMT_OP_READ_VERSION) failed\n";

	if (!mgmt_register(mgmt_master, MGMT_EV_DEVICE_CONNECTED, mgmt_ind, mgmt_device_connected, NULL, nullptr))
		std::cerr << "mgmt_register(MGMT_EV_DEVICE_CONNECTED) failed\n";

	if (!mgmt_register(mgmt_master, MGMT_EV_DISCOVERING, mgmt_ind, mgmt_scanning, NULL, nullptr))
		std::cerr << "mgmt_register(MGMT_EV_DISCOVERING) failed\n";

	if (!mgmt_register(mgmt_master, MGMT_EV_DEVICE_FOUND, mgmt_ind, mgmt_device_found, NULL, nullptr))
		std::cerr << "mgmt_register(MGMT_EV_DEVICE_FOUND) failed\n";
}

bool scan(bool start) {
	mgmt_cp_start_discovery cp {(1 << BDADDR_LE_PUBLIC) | (1 << BDADDR_LE_RANDOM)};
	uint16_t opcode = start? MGMT_OP_START_DISCOVERY : MGMT_OP_STOP_DISCOVERY;

	if (!mgmt_master)
		return false;

	if (mgmt_send(mgmt_master, opcode, mgmt_ind, sizeof(cp), &cp, scan_cb, nullptr, nullptr) == 0) {
		std::cerr << "mgmt_send(MGMT_OP_" << (start? "START" : "STOP") << "_DISCOVERY) failed\n";
		return false;
	}

	return true;
}

static void gatts_mtu_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t mtu, olen;
	size_t plen;

	assert(len >= 3);
	opcode = pdu[0];

	if (!dec_mtu_req(pdu, len, &mtu)) {
		DBG("dec_mtu_req returned false");
		return;
	}

	opdu = g_attrib_get_buffer(attrib, &plen);

	// According to the Bluetooth specification, we're supposed to send the response
	// before applying the new MTU value:
	//   This ATT_MTU value shall be applied in the server after this response has
	//   been sent and before any other Attribute protocol PDU is sent.
	// But if we do it in that order, what happens if setting the MTU fails?

	// set new value for MTU
	if (g_attrib_set_mtu(attrib, mtu)) {
		opt_mtu = mtu;
		olen = enc_mtu_resp(mtu, opdu, plen);
	} else {
		// send NOT SUPPORTED
		olen = enc_error_resp(opcode, mtu, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	}

	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t evt;
	uint16_t handle, olen;
	size_t plen;

	evt = pdu[0];

	if (evt != ATT_OP_HANDLE_NOTIFY && evt != ATT_OP_HANDLE_IND) {
		printf("#Invalid opcode %02X in event handler??\n", evt);
		return;
	}

	assert(len >= 3);
	handle = bt_get_le16(&pdu[1]);

	if (evt == ATT_OP_HANDLE_NOTIFY)
		return;

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_confirmation(opdu, plen);

	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, nullptr, nullptr, nullptr);

	DBG("olen %d", olen);
}

static void gatts_find_info_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len == 5);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_find_by_type_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len >= 7);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_read_by_type_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len == 7 || len == 21);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_read_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle, olen;
	size_t plen;

	assert(len == 3);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_read_blob_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle, olen;
	size_t plen;

	assert(len == 5);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_read_multi_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle1, olen;
	size_t plen;

	assert(len >= 5);
	opcode = pdu[0];
	handle1 = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, handle1, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_read_by_group_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len >= 7);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_write_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle, olen;
	size_t plen;

	assert(len >= 3);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_write_cmd(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	assert(len >= 3);
}

static void gatts_signed_write_cmd(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	assert(len >= 15);
}

static void gatts_prep_write_req(const uint8_t *pdu, uint16_t len, gpointer user_data) { uint8_t *opdu;
	uint8_t opcode, handle;
	uint16_t olen;
	size_t plen;

	assert(len >= 5);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void gatts_exec_write_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t olen;
	size_t plen;

	assert(len == 5);
	opcode = pdu[0];

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_error_resp(opcode, 0, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, nullptr);
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
	uint16_t mtu;
	uint16_t cid;
	GError *gerr = nullptr;

	if (err) {
		set_state(STATE_DISCONNECTED);
		DBG("# Connection error: %s", err->message);
		return;
	}

	bt_io_get(io, &gerr, BT_IO_OPT_IMTU, &mtu, BT_IO_OPT_CID, &cid, BT_IO_OPT_INVALID);

	if (gerr) {
		DBG("# Can't detect MTU, using default");
		g_error_free(gerr);
		mtu = ATT_DEFAULT_LE_MTU;
	} else if (cid == ATT_CID)
		mtu = ATT_DEFAULT_LE_MTU;

	attrib = g_attrib_new(iochannel, mtu, false);

	g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES, events_handler, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES, events_handler, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_FIND_INFO_REQ, GATTRIB_ALL_HANDLES, gatts_find_info_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_FIND_BY_TYPE_REQ, GATTRIB_ALL_HANDLES, gatts_find_by_type_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_BY_TYPE_REQ, GATTRIB_ALL_HANDLES, gatts_read_by_type_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_REQ, GATTRIB_ALL_HANDLES, gatts_read_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_BLOB_REQ, GATTRIB_ALL_HANDLES, gatts_read_blob_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_MULTI_REQ, GATTRIB_ALL_HANDLES, gatts_read_multi_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_BY_GROUP_REQ, GATTRIB_ALL_HANDLES, gatts_read_by_group_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_WRITE_REQ, GATTRIB_ALL_HANDLES, gatts_write_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_WRITE_CMD, GATTRIB_ALL_HANDLES, gatts_write_cmd, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_SIGNED_WRITE_CMD, GATTRIB_ALL_HANDLES, gatts_signed_write_cmd, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_PREP_WRITE_REQ, GATTRIB_ALL_HANDLES, gatts_prep_write_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_EXEC_WRITE_REQ, GATTRIB_ALL_HANDLES, gatts_exec_write_req, attrib, nullptr);
	g_attrib_register(attrib, ATT_OP_MTU_REQ, GATTRIB_ALL_HANDLES, gatts_mtu_req, attrib, nullptr);

	set_state(STATE_CONNECTED);
	connected = true;
	connect_pair.notify();
}

static void disconnect_io() {
	if (conn_state == STATE_DISCONNECTED)
		return;

	g_attrib_unref(attrib);
	attrib = NULL;
	opt_mtu = 0;

	g_io_channel_shutdown(iochannel, FALSE, nullptr);
	g_io_channel_unref(iochannel);
	iochannel = NULL;

	set_state(STATE_DISCONNECTED);
}

static gboolean channel_watcher(GIOChannel *chan, GIOCondition cond, gpointer user_data) {
	DBG("chan = %p", chan);

	// in case of quick disconnection/reconnection, do not mix them
	if (chan == iochannel)
		disconnect_io();

	return FALSE;
}

static bool connect_device(const char *addr, const char *type = "random") {
	set_state(STATE_CONNECTING);
	GError *gerr = nullptr;
	iochannel = gatt_connect(nullptr, addr, type, "low", 0, opt_mtu, connect_cb, &gerr);
	DBG("gatt_connect returned %p", iochannel);
	if (iochannel == nullptr) {
		set_state(STATE_DISCONNECTED);
		g_error_free(gerr);
		return false;
	}

	g_io_add_watch(iochannel, static_cast<GIOCondition>(G_IO_HUP | G_IO_NVAL), channel_watcher, nullptr);
	return true;
}

static void primary_by_uuid_cb(uint8_t status, GSList *ranges, void *user_data) {
	if (status) {
		DBG("status returned error: %s (0x%02x)", att_ecode2str(status), status);
		return;
	}

	for (GSList *l = ranges; l; l = l->next) {
		auto *range = reinterpret_cast<att_range *>(l->data);
		services.emplace_back(range->start, range->end);
	}

	services_pair.notify();
}

static bool cmd_primary(const char *uuid_str) {
	bt_uuid_t uuid;

	if (conn_state != STATE_CONNECTED) {
		DBG("Primary: bad state (%d)", conn_state);
		return false;
	}

	if (bt_string_to_uuid(&uuid, uuid_str) < 0) {
		DBG("Primary: bad param");
		return false;
	}

	gatt_discover_primary(attrib, &uuid, primary_by_uuid_cb, nullptr);
	return true;
}


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

std::atomic_size_t nextCharacteristic {0};
std::map<size_t, CharacteristicEntry> characteristicMap;

static void char_cb(uint8_t status, GSList *characteristics, void *user_data) {
	GSList *l;

	if (status) {
		DBG("status returned error: %s (0x%02x)", att_ecode2str(status), status);
		return;
	}

	auto &entry = characteristicMap.at((size_t) user_data);
	auto &vec = entry.characteristics;

	for (l = characteristics; l; l = l->next) {
		const auto &chars = *reinterpret_cast<gatt_char *>(l->data);
		vec.emplace_back(chars.handle, chars.properties, chars.value_handle, chars.uuid);
	}

	entry.pair.notify();
}

static std::optional<std::vector<Characteristic>> getCharacteristics(uint16_t start = 1, uint16_t end = 0xffff, const char *uuid = nullptr) {
	if (conn_state != STATE_CONNECTED)
		throw std::runtime_error("Invalid state");

	if (uuid != nullptr) {
		bt_uuid_t bt_uuid;

		if (bt_string_to_uuid(&bt_uuid, uuid) < 0) {
			DBG("getCharacteristics: bad param");
			return std::nullopt;
		}

		size_t char_id = nextCharacteristic++;
		auto &characteristics = characteristicMap.try_emplace(char_id).first->second;
		gatt_discover_char(attrib, start, end, &bt_uuid, char_cb, (void *) char_id);
		if (!characteristics.pair.wait_for(std::chrono::milliseconds(5'000)))
			return std::nullopt;
		return std::move(characteristics.characteristics);
	}

	size_t char_id = nextCharacteristic++;
	auto &characteristics = characteristicMap.try_emplace(char_id).first->second;
	gatt_discover_char(attrib, start, end, nullptr, char_cb, (void *) char_id);
	if (!characteristics.pair.wait_for(std::chrono::milliseconds(5'000)))
		return std::nullopt;
	return std::move(characteristics.characteristics);
}

template <typename C>
static std::string toHex(const C &bytes) {
	std::ostringstream ss;
	for (const uint8_t byte: bytes)
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
	return ss.str();
}

template <typename C>
static bool writeBytes(const Characteristic &characteristic, const C &bytes) {
	uint8_t *value = nullptr;
	size_t plen;
	int handle = characteristic.valueHandle;

	if (conn_state != STATE_CONNECTED) {
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

static void toHex(uint8_t byte, char out[3]) {
	uint8_t first = byte >> 4;
	uint8_t second = byte & 0xf;
	if (9 < first)
		out[0] = 'a' + (first - 10);
	else
		out[0] = '0' + first;
	if (9 < second)
		out[1] = 'a' + (second - 10);
	else
		out[1] = '0' + second;
	out[2] = '\0';
}

static bool writeByte(const Characteristic &characteristic, char byte) {
	uint8_t *value = nullptr;
	size_t plen;
	int handle = characteristic.valueHandle;

	if (conn_state != STATE_CONNECTED) {
		DBG("writeBytes: bad state");
		return false;
	}

	if (handle <= 0) {
		DBG("writeBytes: invalid handle");
		return false;
	}

	char buf[3];
	toHex(byte, buf);

	plen = gatt_attr_data_from_string(buf, &value);
	if (plen == 0) {
		DBG("writeBytes: plen == 0");
		return false;
	}

	gatt_write_cmd(attrib, handle, value, plen, nullptr, nullptr);
	g_free(value);

	return true;
}

static GMainLoop *event_loop = nullptr;

uint8_t fromHex(std::string_view str) {
	const uint8_t first  = str[0];
	const uint8_t second = str[1];
	uint8_t out = 0;
	if ('a' <= first && first <= 'f')
		out |= (first - 'a' + 0xa) << 4;
	else
		out |= (first - '0') << 4;
	if ('a' <= second && second <= 'f')
		out |= second - 'a' + 0xa;
	else
		out |= second - '0';
	return out;
}

const Characteristic *rxptr = nullptr;

int main() {
	mgmt_setup(0);

	event_loop = g_main_loop_new(nullptr, false);
	bool running = true;

	std::thread th([&] {
		if (!connect_device("EB:9F:86:0E:E2:F4"))
			return;

		connect_pair.wait_for(std::chrono::milliseconds(5'000), [] {
			return connected.load() && conn_state == STATE_CONNECTED;
		});

		if (!connected || conn_state != STATE_CONNECTED) {
			DBG("Failed to connect. Farewell, world! (%d, %d)", connected.load(), conn_state);
			return;
		}

		DBG("Connected.");

		if (!cmd_primary("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")) {
			DBG("Primary failed.");
			return;
		}

		DBG("Primary succeeded.");

		if (!services_pair.wait_for(std::chrono::milliseconds(5'000))) {
			DBG("Couldn't find services.");
			return;
		}

		DBG("Found services.");
		assert(services.size() == 1);

		auto char_opt = getCharacteristics();
		if (!char_opt) {
			DBG("Couldn't get characteristics.");
			return;
		}

		auto characteristics = std::move(*char_opt);
		rxptr = nullptr;
		for (const auto &charac: characteristics)
			if (charac.uuid == "6e400002-b5a3-f393-e0a9-e50e24dcca9e") {
				rxptr = &charac;
				break;
			}

		if (rxptr == nullptr) {
			DBG("Couldn't find RX characteristic.");
			for (const auto &charac: characteristics)
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

		const auto enc1 = Chemion::encode(image1);
		const auto enc2 = Chemion::encode(image2);

		auto batch = [&](const auto &enc, size_t count) -> bool {
			size_t i = 0;
			std::vector<uint8_t> bytes;
			bytes.reserve(count);
			while (i + count < enc.size()) {
				for (size_t j = 0; j < count; ++j)
					bytes.push_back(enc[i++]);
				if (!writeBytes(rx, bytes)) {
					DBG("Writing failed.");
					return false;
				}
				bytes.clear();
			}

			while (i < enc.size())
				bytes.push_back(enc[i++]);

			if (!bytes.empty() && !writeBytes(rx, bytes)) {
				DBG("Final write failed.");
				return false;
			}

			return true;
		};

		DBG("Entering loop.");

		for (size_t i = 0; i < 10000; ++i) {
			if (!batch(i % 2? enc1 : enc2, 20))
				return;
			std::this_thread::sleep_for(std::chrono::milliseconds(75));
		}

		DBG("Writing succeeded.");
	});

	DBG("Starting loop");
	g_main_loop_run(event_loop);
	DBG("Exiting loop");

	running = false;
	th.join();
}

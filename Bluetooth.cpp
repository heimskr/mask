#include <cassert>
#include <glib.h>

extern "C" {
#include "lib/bluetooth.h"
#include "lib/sdp.h"
#include "lib/uuid.h"
#include "btio/btio.h"
#include "attrib/att.h"
#include "attrib/gattrib.h"
#include "attrib/gatt.h"
#include "attrib/gatttool.h"
};

#include "Bluetooth.h"

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

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t evt;
	uint16_t handle, olen;
	size_t plen;

	evt = pdu[0];

	if (evt != ATT_OP_HANDLE_NOTIFY && evt != ATT_OP_HANDLE_IND) {
		DBG("# Invalid opcode %02x in event handler?", evt);
		return;
	}

	assert(len >= 3);
	handle = bt_get_le16(&pdu[1]);

	if (evt == ATT_OP_HANDLE_NOTIFY)
		return;

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_confirmation(opdu, plen);

	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);

	DBG("olen %d", olen);
}

static void gatts_find_info_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len == 5);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_find_by_type_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len >= 7);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_read_by_type_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len == 7 || len == 21);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_read_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle, olen;
	size_t plen;

	assert(len == 3);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_read_blob_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle, olen;
	size_t plen;

	assert(len == 5);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_read_multi_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle1, olen;
	size_t plen;

	assert(len >= 5);
	opcode = pdu[0];
	handle1 = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, handle1, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_read_by_group_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t starting_handle, olen;
	size_t plen;

	assert(len >= 7);
	opcode = pdu[0];
	starting_handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, starting_handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_write_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t handle, olen;
	size_t plen;

	assert(len >= 3);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_write_cmd(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	assert(len >= 3);
}

static void gatts_signed_write_cmd(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	assert(len >= 15);
}

static void gatts_prep_write_req(const uint8_t *pdu, uint16_t len, gpointer user_data) { uint8_t *opdu;
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t opcode, handle;
	uint16_t olen;
	size_t plen;

	assert(len >= 5);
	opcode = pdu[0];
	handle = bt_get_le16(&pdu[1]);

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, handle, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_exec_write_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint8_t *opdu;
	uint8_t opcode;
	uint16_t olen;
	size_t plen;

	assert(len == 5);
	opcode = pdu[0];

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);
	olen = enc_error_resp(opcode, 0, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void gatts_mtu_req(const uint8_t *pdu, uint16_t len, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
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

	opdu = g_attrib_get_buffer(bluetooth.attrib, &plen);

	// According to the Bluetooth specification, we're supposed to send the response
	// before applying the new MTU value:
	//   This ATT_MTU value shall be applied in the server after this response has
	//   been sent and before any other Attribute protocol PDU is sent.
	// But if we do it in that order, what happens if setting the MTU fails?

	// set new value for MTU
	if (g_attrib_set_mtu(bluetooth.attrib, mtu)) {
		bluetooth.opt_mtu = mtu;
		olen = enc_mtu_resp(mtu, opdu, plen);
	} else {
		// send NOT SUPPORTED
		olen = enc_error_resp(opcode, mtu, ATT_ECODE_REQ_NOT_SUPP, opdu, plen);
	}

	if (olen > 0)
		g_attrib_send(bluetooth.attrib, 0, opdu, olen, nullptr, nullptr, nullptr);
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	uint16_t mtu;
	uint16_t cid;
	GError *gerr = nullptr;

	if (err) {
		bluetooth.setDisconnected();
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

	bluetooth.attrib = g_attrib_new(bluetooth.iochannel, mtu, false);
	auto *attrib = bluetooth.attrib;

	g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES, events_handler, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES, events_handler, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_FIND_INFO_REQ, GATTRIB_ALL_HANDLES, gatts_find_info_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_FIND_BY_TYPE_REQ, GATTRIB_ALL_HANDLES, gatts_find_by_type_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_BY_TYPE_REQ, GATTRIB_ALL_HANDLES, gatts_read_by_type_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_REQ, GATTRIB_ALL_HANDLES, gatts_read_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_BLOB_REQ, GATTRIB_ALL_HANDLES, gatts_read_blob_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_MULTI_REQ, GATTRIB_ALL_HANDLES, gatts_read_multi_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_READ_BY_GROUP_REQ, GATTRIB_ALL_HANDLES, gatts_read_by_group_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_WRITE_REQ, GATTRIB_ALL_HANDLES, gatts_write_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_WRITE_CMD, GATTRIB_ALL_HANDLES, gatts_write_cmd, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_SIGNED_WRITE_CMD, GATTRIB_ALL_HANDLES, gatts_signed_write_cmd, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_PREP_WRITE_REQ, GATTRIB_ALL_HANDLES, gatts_prep_write_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_EXEC_WRITE_REQ, GATTRIB_ALL_HANDLES, gatts_exec_write_req, user_data, nullptr);
	g_attrib_register(attrib, ATT_OP_MTU_REQ, GATTRIB_ALL_HANDLES, gatts_mtu_req, user_data, nullptr);

	bluetooth.setConnected();
	bluetooth.connected = true;
	bluetooth.cvConnect.notify();
}

static gboolean channel_watcher(GIOChannel *chan, GIOCondition cond, gpointer user_data) {
	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);
	DBG("chan = %p", chan);

	// in case of quick disconnection/reconnection, do not mix them
	if (chan == bluetooth.iochannel)
		bluetooth.disconnectIO();

	return FALSE;
}

static void primary_by_uuid_cb(uint8_t status, GSList *ranges, void *user_data) {
	if (status) {
		DBG("status returned error: %s (0x%02x)", att_ecode2str(status), status);
		return;
	}

	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);

	for (GSList *l = ranges; l; l = l->next) {
		auto *range = reinterpret_cast<att_range *>(l->data);
		bluetooth.services.emplace_back(range->start, range->end);
	}

	bluetooth.cvServices.notify();
}

static void char_cb(uint8_t status, GSList *characteristics, void *user_data) {
	GSList *l;

	if (status) {
		DBG("status returned error: %s (0x%02x)", att_ecode2str(status), status);
		return;
	}

	Bluetooth &bluetooth = *reinterpret_cast<Bluetooth *>(user_data);

	assert(bluetooth.nextEntry != nullptr);
	auto &entry = *bluetooth.nextEntry;
	auto &vec = entry.characteristics;

	for (l = characteristics; l; l = l->next) {
		const auto &chars = *reinterpret_cast<gatt_char *>(l->data);
		vec.emplace_back(chars.handle, chars.properties, chars.value_handle, chars.uuid);
	}

	entry.pair.notify();
}

void Bluetooth::setup(uint16_t index) {
	mgmt.setup(index);
}

bool Bluetooth::connectDevice(const char *addr, const char *type) {
	mgmt.state = Mgmt::State::Connecting;

	GError *gerr = nullptr;
	iochannel = gatt_connect(nullptr, addr, type, "low", 0, opt_mtu, connect_cb, this, &gerr);
	DBG("gatt_connect returned %p", iochannel);
	if (iochannel == nullptr) {
		mgmt.state = Mgmt::State::Disconnected;
		g_error_free(gerr);
		return false;
	}

	g_io_add_watch(iochannel, static_cast<GIOCondition>(G_IO_HUP | G_IO_NVAL), channel_watcher, this);
	return true;
}

void Bluetooth::setDisconnected() {
	mgmt.state = Mgmt::State::Disconnected;
}

void Bluetooth::setConnected() {
	mgmt.state = Mgmt::State::Connected;
}

void Bluetooth::disconnectIO() {
	if (mgmt.state == Mgmt::State::Disconnected)
		return;

	g_attrib_unref(attrib);
	attrib = nullptr;
	opt_mtu = 0;

	g_io_channel_shutdown(iochannel, false, nullptr);
	g_io_channel_unref(iochannel);
	iochannel = nullptr;

	setDisconnected();
}

bool Bluetooth::primary(const char *uuid) {
	bt_uuid_t bt_uuid;

	if (mgmt.state != Mgmt::State::Connected) {
		DBG("Primary: bad state (%d)", static_cast<int>(mgmt.state));
		return false;
	}

	if (bt_string_to_uuid(&bt_uuid, uuid) < 0) {
		DBG("Primary: bad param");
		return false;
	}

	gatt_discover_primary(attrib, &bt_uuid, primary_by_uuid_cb, this);
	return true;
}

Characteristic * Bluetooth::findCharacteristic(std::string_view uuid) {
	if (!characteristics)
		throw std::runtime_error("Characteristics list missing");

	for (auto &characteristic: *characteristics)
		if (characteristic.uuid == uuid)
			return &characteristic;

	return nullptr;
}

bool Bluetooth::waitForConnection(size_t milliseconds) {
	return cvConnect.wait_for(std::chrono::milliseconds(milliseconds), [&] {
		return connected.load() && mgmt.state == Mgmt::State::Connected;
	});
}

bool Bluetooth::waitForServices(size_t milliseconds) {
	return cvServices.wait_for(std::chrono::milliseconds(milliseconds));
}

bool Bluetooth::findCharacteristics(uint16_t start, uint16_t end, const char *uuid) {
	if (mgmt.state != Mgmt::State::Connected)
		throw std::runtime_error("Invalid state");

	if (uuid != nullptr) {
		bt_uuid_t bt_uuid;

		if (bt_string_to_uuid(&bt_uuid, uuid) < 0) {
			DBG("getCharacteristics: bad param");
			return false;
		}

		size_t char_id = nextCharacteristic++;
		auto &entry = characteristicMap.try_emplace(char_id).first->second;
		nextEntry = &entry;
		gatt_discover_char(attrib, start, end, &bt_uuid, char_cb, this);
		if (!entry.pair.wait_for(std::chrono::milliseconds(5'000)))
			return false;
		characteristics = std::move(entry.characteristics);
		return true;
	}

	size_t char_id = nextCharacteristic++;
	auto &entry = characteristicMap.try_emplace(char_id).first->second;
	nextEntry = &entry;
	gatt_discover_char(attrib, start, end, nullptr, char_cb, this);
	if (!entry.pair.wait_for(std::chrono::milliseconds(5'000)))
		return false;
	characteristics = std::move(entry.characteristics);
	return true;
}

bool Bluetooth::writeByte(const Characteristic &characteristic, uint8_t byte) {
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

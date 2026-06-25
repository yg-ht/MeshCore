#if defined(NRF52_PLATFORM)

#include "MeshCoreNrf52Dfu.h"

#include <stddef.h>
#include <string.h>

#define DFU_REV_APPMODE 0x0001

/*
 * Standard Adafruit/Nordic legacy DFU service UUIDs. The stock Adafruit
 * BLEDfu service jumps directly into the bootloader image after START_DFU.
 * MeshCore uses a reset instead so OTAFIX and board bootloaders get the same
 * clean reset-time state as a physical reset button press.
 */
static const uint8_t UUID128_SVC_DFU_OTA[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x30, 0x15, 0x00, 0x00};

static const uint8_t UUID128_CHR_DFU_CONTROL[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x31, 0x15, 0x00, 0x00};

static const uint8_t UUID128_CHR_DFU_PACKET[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x32, 0x15, 0x00, 0x00};

static const uint8_t UUID128_CHR_DFU_REVISION[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x34, 0x15, 0x00, 0x00};

static uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;

  while (length-- > 0) {
    uint8_t x = (uint8_t)((crc >> 8) ^ *data++);
    x ^= (uint8_t)(x >> 4);
    crc = (uint16_t)((crc << 8) ^ ((uint16_t)x << 12) ^ ((uint16_t)x << 5) ^ x);
  }

  return crc;
}

static void save_peer_data_for_bootloader(uint16_t conn_handle, BLEConnection *conn) {
  /*
   * This structure and RAM address are part of Adafruit's legacy OTA DFU
   * bootloader contract. The bootloader reads this handoff after reset so it
   * can continue with the same central instead of requiring a second manual
   * scan/selection.
   */
  typedef struct {
    ble_gap_addr_t addr;
    ble_gap_irk_t irk;
    ble_gap_enc_key_t enc_key;
    uint8_t sys_attr[8];
    uint16_t crc16;
  } peer_data_t;

  static_assert(offsetof(peer_data_t, crc16) == 60,
                "nRF52 OTA peer-data layout must match the bootloader");

  peer_data_t *peer_data = (peer_data_t *)(0x20007F80UL);
  memset(peer_data, 0, sizeof(peer_data_t));

  uint16_t sysattr_len = sizeof(peer_data->sys_attr);
  sd_ble_gatts_sys_attr_get(conn_handle, peer_data->sys_attr, &sysattr_len,
                            BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS);

  peer_data->addr = conn->getPeerAddr();

  if (conn->secured()) {
    bond_keys_t bond_keys;
    if (conn->loadBondKey(&bond_keys)) {
      peer_data->addr = bond_keys.peer_id.id_addr_info;
      peer_data->irk = bond_keys.peer_id.id_info;
      peer_data->enc_key = bond_keys.own_enc;
    }
  }

  peer_data->crc16 = crc16((uint8_t *)peer_data, offsetof(peer_data_t, crc16));
}

static void dfu_control_write_authorize(uint16_t conn_handle, BLECharacteristic *chr,
                                        ble_gatts_evt_write_t *request) {
  if ((request->handle != chr->handles().value_handle) ||
      (request->op == BLE_GATTS_OP_PREP_WRITE_REQ) ||
      (request->op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
      (request->op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL)) {
    return;
  }

  ble_gatts_rw_authorize_reply_params_t reply = {.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE};

  if (!chr->notifyEnabled(conn_handle)) {
    reply.params.write.gatt_status = BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR;
    sd_ble_gatts_rw_authorize_reply(conn_handle, &reply);
    return;
  }

  reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
  sd_ble_gatts_rw_authorize_reply(conn_handle, &reply);

  enum { START_DFU = 1 };
  if (request->len == 0 || request->data[0] != START_DFU) {
    return;
  }

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (conn == nullptr) {
    return;
  }

  save_peer_data_for_bootloader(conn_handle, conn);

  // From this point the device should not fall back into normal app BLE
  // advertising; the reset below transfers control to the OTA bootloader.
  Bluefruit.Advertising.restartOnDisconnect(false);
  conn->disconnect();
  enterOTADfu();
}

MeshCoreNrf52Dfu::MeshCoreNrf52Dfu()
    : BLEService(UUID128_SVC_DFU_OTA), _chr_control(UUID128_CHR_DFU_CONTROL) {}

err_t MeshCoreNrf52Dfu::begin() {
  VERIFY_STATUS(BLEService::begin());

  BLECharacteristic chr_packet(UUID128_CHR_DFU_PACKET);
  chr_packet.setTempMemory();
  chr_packet.setProperties(CHR_PROPS_WRITE_WO_RESP);
  chr_packet.setMaxLen(20);
  VERIFY_STATUS(chr_packet.begin());

  _chr_control.setProperties(CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
  _chr_control.setMaxLen(23);
  _chr_control.setWriteAuthorizeCallback(dfu_control_write_authorize);
  VERIFY_STATUS(_chr_control.begin());

  BLECharacteristic chr_revision(UUID128_CHR_DFU_REVISION);
  chr_revision.setTempMemory();
  chr_revision.setProperties(CHR_PROPS_READ);
  chr_revision.setFixedLen(2);
  VERIFY_STATUS(chr_revision.begin());
  chr_revision.write16(DFU_REV_APPMODE);

  return ERROR_NONE;
}

#endif

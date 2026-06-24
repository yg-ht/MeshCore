#if defined(NRF52_PLATFORM)

#include "MeshCoreNrf52Dfu.h"

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

static SoftwareTimer dfu_reset_timer;
static bool dfu_reset_timer_initialised = false;

static void reset_to_ota_dfu(TimerHandle_t timer) {
  (void)timer;

  enterOTADfu();
}

static void schedule_reset_to_ota_dfu() {
  if (!dfu_reset_timer_initialised) {
    dfu_reset_timer.begin(250, reset_to_ota_dfu, nullptr, false);
    dfu_reset_timer_initialised = true;
  }

  dfu_reset_timer.start();
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

  Bluefruit.Advertising.restartOnDisconnect(false);

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (conn != nullptr) {
    conn->disconnect();
  }

  schedule_reset_to_ota_dfu();
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

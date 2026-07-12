#pragma once

#include "Mesh.h"
#include <RadioLib.h>
#include <stdio.h>
#include <string.h>

class StatsFormatHelper {
  struct PacketErrorStatus {
    int16_t code;
    const char* key;
    bool is_message_error;
  };

  enum { CLI_REPLY_LIMIT = 160 };

  static bool appendField(char*& out, size_t& remaining, const char* key, uint32_t count) {
    char field[64];
    snprintf(field, sizeof(field), ",\"%s\":%u", key, count);
    size_t len = strlen(field);
    if (len + 1 > remaining) {
      return false;
    }
    memcpy(out, field, len + 1);
    out += len;
    remaining -= len;
    return true;
  }

  static const PacketErrorStatus* findPacketErrorStatus(int16_t code) {
    static const PacketErrorStatus statuses[] = {
      { RADIOLIB_ERR_PACKET_TOO_LONG, "packet_too_long", true },
      { RADIOLIB_ERR_TX_TIMEOUT, "tx_timeout", true },
      { RADIOLIB_ERR_RX_TIMEOUT, "rx_timeout", true },
      { RADIOLIB_ERR_CRC_MISMATCH, "crc_mismatch", true },
      { RADIOLIB_ERR_LORA_HEADER_DAMAGED, "lora_header_damaged", true },
      { RADIOLIB_ERR_PACKET_TOO_SHORT, "packet_too_short", true },

      { RADIOLIB_ERR_UNKNOWN, "unknown_error", false },
      { RADIOLIB_ERR_CHIP_NOT_FOUND, "chip_not_found", false },
      { RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED, "memory_allocation_failed", false },
      { RADIOLIB_ERR_INVALID_BANDWIDTH, "invalid_bandwidth", false },
      { RADIOLIB_ERR_INVALID_SPREADING_FACTOR, "invalid_spreading_factor", false },
      { RADIOLIB_ERR_INVALID_CODING_RATE, "invalid_coding_rate", false },
      { RADIOLIB_ERR_INVALID_BIT_RANGE, "invalid_bit_range", false },
      { RADIOLIB_ERR_INVALID_FREQUENCY, "invalid_frequency", false },
      { RADIOLIB_ERR_INVALID_OUTPUT_POWER, "invalid_output_power", false },
      { RADIOLIB_ERR_SPI_WRITE_FAILED, "spi_write_failed", false },
      { RADIOLIB_ERR_INVALID_CURRENT_LIMIT, "invalid_current_limit", false },
      { RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH, "invalid_preamble_length", false },
      { RADIOLIB_ERR_INVALID_GAIN, "invalid_gain", false },
      { RADIOLIB_ERR_WRONG_MODEM, "wrong_modem", false },
      { RADIOLIB_ERR_INVALID_NUM_SAMPLES, "invalid_num_samples", false },
      { RADIOLIB_ERR_INVALID_RSSI_OFFSET, "invalid_rssi_offset", false },
      { RADIOLIB_ERR_INVALID_ENCODING, "invalid_encoding", false },
      { RADIOLIB_ERR_UNSUPPORTED, "unsupported", false },
      { RADIOLIB_ERR_INVALID_DIO_PIN, "invalid_dio_pin", false },
      { RADIOLIB_ERR_INVALID_RSSI_THRESHOLD, "invalid_rssi_threshold", false },
      { RADIOLIB_ERR_NULL_POINTER, "null_pointer", false },
      { RADIOLIB_ERR_INVALID_IRQ, "invalid_irq", false },

      { RADIOLIB_ERR_INVALID_CRC_CONFIGURATION, "invalid_crc_configuration", false },
      { RADIOLIB_ERR_INVALID_TCXO_VOLTAGE, "invalid_tcxo_voltage", false },
      { RADIOLIB_ERR_INVALID_MODULATION_PARAMETERS, "invalid_modulation_parameters", false },
      { RADIOLIB_ERR_SPI_CMD_TIMEOUT, "spi_cmd_timeout", false },
      { RADIOLIB_ERR_SPI_CMD_INVALID, "spi_cmd_invalid", false },
      { RADIOLIB_ERR_SPI_CMD_FAILED, "spi_cmd_failed", false },
      { RADIOLIB_ERR_INVALID_SLEEP_PERIOD, "invalid_sleep_period", false },
      { RADIOLIB_ERR_INVALID_RX_PERIOD, "invalid_rx_period", false },
    };

    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
      if (statuses[i].code == code) {
        return &statuses[i];
      }
    }
    return NULL;
  }

  static bool isMessagePacketError(int16_t code) {
    const PacketErrorStatus* status = findPacketErrorStatus(code);
    return status != NULL && status->is_message_error;
  }

public:
  static void formatCoreStats(char* reply, 
                             mesh::MainBoard& board, 
                             mesh::MillisecondClock& ms, 
                             uint16_t err_flags,
                             mesh::PacketManager* mgr) {
    sprintf(reply, 
      "{\"battery_mv\":%u,\"uptime_secs\":%u,\"errors\":%u,\"queue_len\":%u}",
      board.getBattMilliVolts(),
      ms.getMillis() / 1000,
      err_flags,
      mgr->getOutboundTotal()
    );
  }

  template<typename RadioDriverType>
  static void formatRadioStats(char* reply,
                              mesh::Radio* radio,
                              RadioDriverType& driver,
                              uint32_t total_air_time_ms,
                              uint32_t total_rx_air_time_ms) {
    sprintf(reply, 
      "{\"noise_floor\":%d,\"last_rssi\":%d,\"last_snr\":%.2f,\"tx_air_secs\":%u,\"rx_air_secs\":%u}",
      (int16_t)radio->getNoiseFloor(),
      (int16_t)driver.getLastRSSI(),
      driver.getLastSNR(),
      total_air_time_ms / 1000,
      total_rx_air_time_ms / 1000
    );
  }

  template<typename RadioDriverType>
  static void formatPacketStats(char* reply,
                               RadioDriverType& driver,
                               uint32_t n_sent_flood,
                               uint32_t n_sent_direct,
                               uint32_t n_recv_flood,
                               uint32_t n_recv_direct) {
    sprintf(reply, 
      "{\"recv\":%u,\"sent\":%u,\"flood_tx\":%u,\"direct_tx\":%u,\"flood_rx\":%u,\"direct_rx\":%u,\"recv_errors\":%u}",
      driver.getPacketsRecv(),
      driver.getPacketsSent(),
      n_sent_flood,
      n_sent_direct,
      n_recv_flood,
      n_recv_direct,
      driver.getPacketsRecvErrors()
    );
  }

  template<typename RadioDriverType>
  static void formatPacketErrorStats(char* reply, RadioDriverType& driver, bool message_errors) {
    uint32_t total = 0;
    int16_t last = driver.getLastPacketError();
    char* out = reply;
    size_t remaining = CLI_REPLY_LIMIT;
    bool truncated = false;

    if (message_errors != isMessagePacketError(last)) {
      last = RADIOLIB_ERR_NONE;
    }

    for (uint8_t i = 0; i < driver.getPacketErrorStatusCount(); i++) {
      int16_t code;
      uint32_t count;
      if (!driver.getPacketErrorStatus(i, &code, &count)) {
        continue;
      }

      const PacketErrorStatus* status = findPacketErrorStatus(code);
      bool is_message = status != NULL && status->is_message_error;
      if (message_errors == is_message) {
        total += count;
      }
    }

    int written = snprintf(out, remaining, "{\"total\":%u,\"last\":%d", total, (int)last);
    if (written < 0 || (size_t)written >= remaining) {
      strcpy(reply, "{\"error\":\"reply_too_short\"}");
      return;
    }
    out += written;
    remaining -= written;

    for (uint8_t i = 0; i < driver.getPacketErrorStatusCount(); i++) {
      int16_t code;
      uint32_t count;
      if (!driver.getPacketErrorStatus(i, &code, &count)) {
        continue;
      }

      const PacketErrorStatus* status = findPacketErrorStatus(code);
      bool is_message = status != NULL && status->is_message_error;

      // Unknown RadioLib statuses are still device-side diagnostics because
      // they are not known transmission-quality outcomes.
      if (message_errors != is_message) {
        continue;
      }

      char unknown_key[16];
      const char* key = status ? status->key : unknown_key;
      if (status == NULL) {
        snprintf(unknown_key, sizeof(unknown_key), "code_%d", (int)code);
      }

      if (!appendField(out, remaining, key, count)) {
        truncated = true;
      }
    }

    if (truncated) {
      appendField(out, remaining, "truncated", 1);
    }

    if (remaining > 1) {
      *out++ = '}';
      *out = 0;
    } else {
      reply[CLI_REPLY_LIMIT - 2] = '}';
      reply[CLI_REPLY_LIMIT - 1] = 0;
    }
  }
};

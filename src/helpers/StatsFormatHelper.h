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

  static bool appendField(char*& out, size_t& remaining, const char* key, int32_t value) {
    char field[64];
    const char* separator = out[-1] == '{' ? "" : ",";
    snprintf(field, sizeof(field), "%s\"%s\":%ld", separator, key, (long)value);
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
      { RADIOLIB_ERR_PACKET_TOO_LONG, "pkt_long", true },
      { RADIOLIB_ERR_TX_TIMEOUT, "tx_to", true },
      { RADIOLIB_ERR_RX_TIMEOUT, "rx_to", true },
      { RADIOLIB_ERR_CRC_MISMATCH, "crc", true },
      { RADIOLIB_ERR_LORA_HEADER_DAMAGED, "hdr_bad", true },
      { RADIOLIB_ERR_PACKET_TOO_SHORT, "pkt_short", true },

      { RADIOLIB_ERR_UNKNOWN, "unk", false },
      { RADIOLIB_ERR_CHIP_NOT_FOUND, "no_chip", false },
      { RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED, "no_mem", false },
      { RADIOLIB_ERR_INVALID_BANDWIDTH, "bad_bw", false },
      { RADIOLIB_ERR_INVALID_SPREADING_FACTOR, "bad_sf", false },
      { RADIOLIB_ERR_INVALID_CODING_RATE, "bad_cr", false },
      { RADIOLIB_ERR_INVALID_BIT_RANGE, "bad_bit", false },
      { RADIOLIB_ERR_INVALID_FREQUENCY, "bad_freq", false },
      { RADIOLIB_ERR_INVALID_OUTPUT_POWER, "bad_pwr", false },
      { RADIOLIB_ERR_SPI_WRITE_FAILED, "spi_wr", false },
      { RADIOLIB_ERR_INVALID_CURRENT_LIMIT, "bad_curr", false },
      { RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH, "bad_pre", false },
      { RADIOLIB_ERR_INVALID_GAIN, "bad_gain", false },
      { RADIOLIB_ERR_WRONG_MODEM, "modem", false },
      { RADIOLIB_ERR_INVALID_NUM_SAMPLES, "bad_samp", false },
      { RADIOLIB_ERR_INVALID_RSSI_OFFSET, "bad_rssi_off", false },
      { RADIOLIB_ERR_INVALID_ENCODING, "bad_enc", false },
      { RADIOLIB_ERR_UNSUPPORTED, "unsupported", false },
      { RADIOLIB_ERR_INVALID_DIO_PIN, "bad_dio", false },
      { RADIOLIB_ERR_INVALID_RSSI_THRESHOLD, "bad_rssi_thr", false },
      { RADIOLIB_ERR_NULL_POINTER, "null", false },
      { RADIOLIB_ERR_INVALID_IRQ, "bad_irq", false },

      { RADIOLIB_ERR_INVALID_CRC_CONFIGURATION, "bad_crc_cfg", false },
      { RADIOLIB_ERR_INVALID_TCXO_VOLTAGE, "bad_tcxo", false },
      { RADIOLIB_ERR_INVALID_MODULATION_PARAMETERS, "bad_mod", false },
      { RADIOLIB_ERR_SPI_CMD_TIMEOUT, "spi_to", false },
      { RADIOLIB_ERR_SPI_CMD_INVALID, "spi_bad", false },
      { RADIOLIB_ERR_SPI_CMD_FAILED, "spi_fail", false },
      { RADIOLIB_ERR_INVALID_SLEEP_PERIOD, "bad_sleep", false },
      { RADIOLIB_ERR_INVALID_RX_PERIOD, "bad_rx", false },
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
    char* out = reply;
    size_t remaining = CLI_REPLY_LIMIT;
    bool truncated = false;

    int written = snprintf(out, remaining, "{");
    if (written < 0 || (size_t)written >= remaining) {
      strcpy(reply, "{\"error\":\"reply_too_short\"}");
      return;
    }
    out += written;
    remaining -= written;

    if (message_errors) {
      if (!appendField(out, remaining, "rssi", (int32_t)driver.getLastRSSI())) {
        truncated = true;
      }
      if (!appendField(out, remaining, "snr", (int32_t)driver.getLastSNR())) {
        truncated = true;
      }
    }

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
        snprintf(unknown_key, sizeof(unknown_key), "c%d", (int)code);
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

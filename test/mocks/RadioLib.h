#pragma once

#include <stddef.h>
#include <stdint.h>

#define RADIOLIB_ERR_NONE 0
#define RADIOLIB_ERR_UNKNOWN -1
#define RADIOLIB_ERR_CHIP_NOT_FOUND -2
#define RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED -3
#define RADIOLIB_ERR_PACKET_TOO_LONG -4
#define RADIOLIB_ERR_TX_TIMEOUT -5
#define RADIOLIB_ERR_RX_TIMEOUT -6
#define RADIOLIB_ERR_CRC_MISMATCH -7
#define RADIOLIB_ERR_INVALID_BANDWIDTH -8
#define RADIOLIB_ERR_INVALID_SPREADING_FACTOR -9
#define RADIOLIB_ERR_INVALID_CODING_RATE -10
#define RADIOLIB_ERR_INVALID_BIT_RANGE -11
#define RADIOLIB_ERR_INVALID_FREQUENCY -12
#define RADIOLIB_ERR_INVALID_OUTPUT_POWER -13
#define RADIOLIB_ERR_SPI_WRITE_FAILED -16
#define RADIOLIB_ERR_INVALID_CURRENT_LIMIT -17
#define RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH -18
#define RADIOLIB_ERR_INVALID_GAIN -19
#define RADIOLIB_ERR_WRONG_MODEM -20
#define RADIOLIB_ERR_INVALID_NUM_SAMPLES -21
#define RADIOLIB_ERR_INVALID_RSSI_OFFSET -22
#define RADIOLIB_ERR_INVALID_ENCODING -23
#define RADIOLIB_ERR_LORA_HEADER_DAMAGED -24
#define RADIOLIB_ERR_UNSUPPORTED -25
#define RADIOLIB_ERR_INVALID_DIO_PIN -26
#define RADIOLIB_ERR_INVALID_RSSI_THRESHOLD -27
#define RADIOLIB_ERR_NULL_POINTER -28
#define RADIOLIB_ERR_INVALID_IRQ -29
#define RADIOLIB_ERR_PACKET_TOO_SHORT -30
#define RADIOLIB_ERR_INVALID_CRC_CONFIGURATION -701
#define RADIOLIB_ERR_INVALID_TCXO_VOLTAGE -703
#define RADIOLIB_ERR_INVALID_MODULATION_PARAMETERS -704
#define RADIOLIB_ERR_SPI_CMD_TIMEOUT -705
#define RADIOLIB_ERR_SPI_CMD_INVALID -706
#define RADIOLIB_ERR_SPI_CMD_FAILED -707
#define RADIOLIB_ERR_INVALID_SLEEP_PERIOD -708
#define RADIOLIB_ERR_INVALID_RX_PERIOD -709
#define RADIOLIB_CHANNEL_FREE 0
#define LORA_SF 10

inline long random(long min, long max) {
  return min < max ? min : max;
}

class PhysicalLayer {
public:
  virtual ~PhysicalLayer() = default;
  virtual void setPacketReceivedAction(void (*func)(void)) { (void)func; }
  virtual long random(long max) { return max; }
  virtual void setOutputPower(int8_t dbm) { (void)dbm; }
  virtual void standby() { }
  virtual void sleep() { }
  virtual int16_t setPreambleLength(size_t len) {
    (void)len;
    return RADIOLIB_ERR_NONE;
  }
  virtual int16_t startReceive() { return RADIOLIB_ERR_NONE; }
  virtual size_t getPacketLength(bool update = true) {
    (void)update;
    return 0;
  }
  virtual int16_t readData(uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    return RADIOLIB_ERR_NONE;
  }
  virtual unsigned long getTimeOnAir(size_t len) {
    (void)len;
    return 0;
  }
  virtual int16_t startTransmit(uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    return RADIOLIB_ERR_NONE;
  }
  virtual void finishTransmit() { }
  virtual int16_t scanChannel() { return RADIOLIB_CHANNEL_FREE; }
  virtual uint8_t randomByte() { return 0; }
  virtual float getRSSI() { return RADIOLIB_ERR_UNKNOWN; }
  virtual float getSNR() { return RADIOLIB_ERR_UNKNOWN; }
};

#pragma once

#include <stdint.h>

namespace mesh {

static constexpr uint8_t TRACE_PATH_HASH_MODE_MASK = 0x03;

inline uint8_t getTracePathHashSize(uint8_t flags) {
  // Trace packets use the same path hash mode as ordinary paths:
  // mode 0 = 1 byte, mode 1 = 2 bytes, mode 2 = 3 bytes.
  uint8_t mode = flags & TRACE_PATH_HASH_MODE_MASK;
  return mode < 3 ? mode + 1 : 0;
}

inline uint16_t getTracePathByteOffset(uint16_t hop_count, uint8_t flags) {
  // The packet path stores one SNR byte per completed hop, so its current
  // length is also the number of trace hashes already consumed.
  return hop_count * getTracePathHashSize(flags);
}

inline bool isValidTracePathByteLen(uint8_t byte_len, uint8_t flags) {
  uint8_t hash_size = getTracePathHashSize(flags);
  return hash_size > 0 && (byte_len % hash_size) == 0;
}

inline uint8_t getTracePathHopCount(uint8_t byte_len, uint8_t flags) {
  uint8_t hash_size = getTracePathHashSize(flags);
  return hash_size > 0 ? byte_len / hash_size : 0;
}

}

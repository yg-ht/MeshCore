#pragma once

#include <stdint.h>

// These build flags seed a repeater's preferences only when no persisted value
// is available. Keep the upstream defaults unchanged for ordinary builds.
#ifndef REPEATER_DEFAULT_PATH_HASH_MODE
  #define REPEATER_DEFAULT_PATH_HASH_MODE 0
#endif

#ifndef REPEATER_DEFAULT_LOOP_DETECT
  #define REPEATER_DEFAULT_LOOP_DETECT 0
#endif

#ifndef REPEATER_DEFAULT_TX_DELAY_FACTOR
  #define REPEATER_DEFAULT_TX_DELAY_FACTOR 0.5f
#endif

#ifndef REPEATER_DEFAULT_CAD_ENABLED
  #define REPEATER_DEFAULT_CAD_ENABLED 0
#endif

#ifndef REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL
  #define REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL 47
#endif

// Reject invalid deployment flags during compilation rather than flashing a
// repeater with values that its CLI would refuse to store.
static_assert(REPEATER_DEFAULT_PATH_HASH_MODE >= 0 && REPEATER_DEFAULT_PATH_HASH_MODE <= 2,
              "REPEATER_DEFAULT_PATH_HASH_MODE must be between 0 and 2");
static_assert(REPEATER_DEFAULT_LOOP_DETECT >= 0 && REPEATER_DEFAULT_LOOP_DETECT <= 3,
              "REPEATER_DEFAULT_LOOP_DETECT must be between 0 and 3");
static_assert(REPEATER_DEFAULT_TX_DELAY_FACTOR >= 0.0f && REPEATER_DEFAULT_TX_DELAY_FACTOR <= 2.0f,
              "REPEATER_DEFAULT_TX_DELAY_FACTOR must be between 0 and 2");
static_assert(REPEATER_DEFAULT_CAD_ENABLED == 0 || REPEATER_DEFAULT_CAD_ENABLED == 1,
              "REPEATER_DEFAULT_CAD_ENABLED must be 0 or 1");
static_assert(REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL == 0 ||
                (REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL >= 3 &&
                 REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL <= 168),
              "REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL must be 0 or between 3 and 168");

namespace repeater_build_defaults {

constexpr uint8_t PATH_HASH_MODE = REPEATER_DEFAULT_PATH_HASH_MODE;
constexpr uint8_t LOOP_DETECT = REPEATER_DEFAULT_LOOP_DETECT;
constexpr float TX_DELAY_FACTOR = REPEATER_DEFAULT_TX_DELAY_FACTOR;
constexpr uint8_t CAD_ENABLED = REPEATER_DEFAULT_CAD_ENABLED;
constexpr uint8_t FLOOD_ADVERT_INTERVAL = REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL;

} // namespace repeater_build_defaults

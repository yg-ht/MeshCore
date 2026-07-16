#include <gtest/gtest.h>

// Model the private PlatformIO build flags used for deployed repeaters.
#define REPEATER_DEFAULT_PATH_HASH_MODE 1
#define REPEATER_DEFAULT_LOOP_DETECT 1
#define REPEATER_DEFAULT_TX_DELAY_FACTOR 0.7f
#define REPEATER_DEFAULT_CAD_ENABLED 1
#define REPEATER_DEFAULT_FLOOD_ADVERT_INTERVAL 168

#include "../../examples/simple_repeater/RepeaterBuildDefaults.h"

TEST(RepeaterBuildDefaults, AcceptPrivateDeploymentOverrides) {
  EXPECT_EQ(1, repeater_build_defaults::PATH_HASH_MODE);
  EXPECT_EQ(1, repeater_build_defaults::LOOP_DETECT);
  EXPECT_FLOAT_EQ(0.7f, repeater_build_defaults::TX_DELAY_FACTOR);
  EXPECT_EQ(1, repeater_build_defaults::CAD_ENABLED);
  EXPECT_EQ(168, repeater_build_defaults::FLOOD_ADVERT_INTERVAL);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

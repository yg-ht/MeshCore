#include <gtest/gtest.h>

#include "../../examples/simple_repeater/RepeaterBuildDefaults.h"

TEST(RepeaterBuildDefaults, PreserveUpstreamDefaultsWithoutBuildFlags) {
  EXPECT_EQ(0, repeater_build_defaults::PATH_HASH_MODE);
  EXPECT_EQ(0, repeater_build_defaults::LOOP_DETECT);
  EXPECT_FLOAT_EQ(0.5f, repeater_build_defaults::TX_DELAY_FACTOR);
  EXPECT_EQ(0, repeater_build_defaults::CAD_ENABLED);
  EXPECT_EQ(47, repeater_build_defaults::FLOOD_ADVERT_INTERVAL);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

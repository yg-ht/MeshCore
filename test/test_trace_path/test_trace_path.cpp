#include <gtest/gtest.h>
#include <TracePath.h>

using namespace mesh;

TEST(TracePath, MapsPathHashModesToOrdinaryHashSizes) {
  EXPECT_EQ(1u, getTracePathHashSize(0));
  EXPECT_EQ(2u, getTracePathHashSize(1));
  EXPECT_EQ(3u, getTracePathHashSize(2));
}

TEST(TracePath, IgnoresHigherFlagBitsWhenSelectingHashSize) {
  EXPECT_EQ(3u, getTracePathHashSize(0xFE));
}

TEST(TracePath, RejectsReservedMode) {
  EXPECT_EQ(0u, getTracePathHashSize(3));
  EXPECT_FALSE(isValidTracePathByteLen(12, 3));
  EXPECT_EQ(0u, getTracePathHopCount(12, 3));
}

TEST(TracePath, ValidatesPathBytesAgainstTraceHashSize) {
  EXPECT_TRUE(isValidTracePathByteLen(12, 2));
  EXPECT_EQ(4u, getTracePathHopCount(12, 2));

  EXPECT_FALSE(isValidTracePathByteLen(4, 2));
  EXPECT_FALSE(isValidTracePathByteLen(13, 2));
  EXPECT_EQ(4u, getTracePathHopCount(13, 2));
}

TEST(TracePath, KeepsSingleByteTracePathsValid) {
  EXPECT_TRUE(isValidTracePathByteLen(1, 0));
  EXPECT_TRUE(isValidTracePathByteLen(2, 0));
  EXPECT_TRUE(isValidTracePathByteLen(63, 0));
  EXPECT_EQ(63u, getTracePathHopCount(63, 0));
}

TEST(TracePath, CalculatesOffsetsForThreeByteTraceHops) {
  EXPECT_EQ(0u, getTracePathByteOffset(0, 2));
  EXPECT_EQ(3u, getTracePathByteOffset(1, 2));
  EXPECT_EQ(6u, getTracePathByteOffset(2, 2));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

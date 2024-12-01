#include "unlzx.hh"

#include "gtest/gtest.h"

TEST(CrcCalcTest, HandlesZeroLength) {
  uint8_t memory[1] = {0};
  crc::reset();
  ASSERT_EQ(crc::calc(memory, 0), 0);
  ASSERT_EQ(crc::sum(), 0);
}

TEST(CrcCalcTest, HandlesNonZeroLength) {
  uint32_t expected_sum = 0x55bc801d;

  uint8_t memory[3] = {1, 2, 3};
  crc::reset();

  // Expected value should be calculated based on the crc_table and initial sum
  // This is a placeholder value, replace it with the correct expected value
  ASSERT_EQ(crc::calc(memory, 3), expected_sum);
  ASSERT_EQ(crc::sum(), expected_sum);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#include "crc.hh"

#include <gtest/gtest.h>

namespace {

TEST(CrcCalcTest, HandlesZeroLength) {
  uint8_t memory[1] = {0};
  crc::Crc32 crc;
  ASSERT_EQ(crc.calc(memory, 0), 0);
  ASSERT_EQ(crc.sum(), 0);
}

TEST(CrcCalcTest, HandlesSingleByte) {
  uint8_t memory[1] = {0x01};
  crc::Crc32 crc;
  ASSERT_EQ(crc.calc(memory, 1), 0xA505DF1B);
  ASSERT_EQ(crc.sum(), 0xA505DF1B);
}

TEST(CrcCalcTest, HandlesMultipleBytes) {
  uint8_t memory[3] = {0x01, 0x02, 0x03};
  crc::Crc32 crc;
  ASSERT_EQ(crc.calc(memory, 3), 0x55BC801D);
  ASSERT_EQ(crc.sum(), 0x55BC801D);
}

TEST(CrcCalcTest, HandlesKnownCrcValues) {
  uint8_t memory1[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  crc::Crc32 crc1;
  ASSERT_EQ(crc1.calc(memory1, 4), 0x7C9CA35A);
  ASSERT_EQ(crc1.sum(), 0x7C9CA35A);

  uint8_t memory2[5] = {0x12, 0x34, 0x56, 0x78, 0x9A};
  crc::Crc32 crc2;
  ASSERT_EQ(crc2.calc(memory2, 5), 0x3C4687AF);
  ASSERT_EQ(crc2.sum(), 0x3C4687AF);
}

}  // namespace

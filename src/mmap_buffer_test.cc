#include "mmap_buffer.hh"

#include <gtest/gtest.h>

class TestInputBuffer : public InputBuffer {
 public:
  template <size_t N>
  TestInputBuffer(const uint8_t (&data)[N]) : InputBuffer(data, N) {}

  Status read_word(uint16_t& out) {
    return InputBuffer::read_word(out);
  }
};

TEST(InputBufferTest, ReadWord) {
  const uint8_t kTestData[] = {0x12, 0x34, 0x56, 0x78};
  auto          buffer      = TestInputBuffer(kTestData);

  uint16_t val;
  EXPECT_EQ(buffer.read_word(val), Status::Ok);
  EXPECT_EQ(val, 0x1234);
  EXPECT_EQ(buffer.read_word(val), Status::Ok);
  EXPECT_EQ(val, 0x5678);
}

TEST(InputBufferTest, ReadBits) {
  const uint8_t kTestData[] = {0x12, 0x34, 0x56, 0x78};
  auto          buffer      = TestInputBuffer(kTestData);

  uint16_t val;
  EXPECT_EQ(buffer.read_bits(4, val), Status::Ok);
  EXPECT_EQ(val, 0x4);
  EXPECT_EQ(buffer.read_bits(4, val), Status::Ok);
  EXPECT_EQ(val, 0x3);
  EXPECT_EQ(buffer.read_bits(8, val), Status::Ok);
  EXPECT_EQ(val, 0x12);
  EXPECT_EQ(buffer.read_bits(4, val), Status::Ok);
  EXPECT_EQ(val, 0x8);
  EXPECT_EQ(buffer.read_bits(4, val), Status::Ok);
  EXPECT_EQ(val, 0x7);
  EXPECT_EQ(buffer.read_bits(8, val), Status::Ok);
  EXPECT_EQ(val, 0x56);
}

TEST(InputBufferTest, ReadBitsMoreThan16) {
  const uint8_t kTestData[] = {0x12, 0x34};
  auto          buffer      = TestInputBuffer(kTestData);

  uint16_t val;
  EXPECT_EQ(buffer.read_bits(17, val), Status::OutOfRange);
}

TEST(InputBufferTest, ReadWordOutOfRange) {
  const uint8_t kTestData[] = {0x12};
  auto          buffer      = TestInputBuffer(kTestData);

  uint16_t val;
  EXPECT_EQ(buffer.read_word(val), Status::OutOfRange);
}

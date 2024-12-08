#include "mmap_buffer.hh"

#include <gtest/gtest.h>

class TestInputBuffer : public InputBuffer {
 public:
  template <size_t N>
  TestInputBuffer(const uint8_t (&data)[N]) : InputBuffer(data, N) {}

  uint16_t read_word() {
    return InputBuffer::read_word();
  }
};

// Helper method that, given a message, and a lambda, calls lambda, expecting it to throw a runtime
// error with a specific message.
void expect_runtime_error(const std::string& message, const std::function<void()>& func) {
  try {
    func();
    FAIL() << "Expected std::runtime_error";
  } catch (const std::runtime_error& e) {
    EXPECT_EQ(e.what(), message);
  } catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST(InputBufferTest, ReadWord) {
  const uint8_t kTestData[] = {0x12, 0x34, 0x56, 0x78};
  auto          buffer      = TestInputBuffer(kTestData);

  EXPECT_EQ(buffer.read_word(), 0x1234);
  EXPECT_EQ(buffer.read_word(), 0x5678);
}

TEST(InputBufferTest, ReadBits) {
  const uint8_t kTestData[] = {0x12, 0x34, 0x56, 0x78};
  auto          buffer      = TestInputBuffer(kTestData);

  EXPECT_EQ(buffer.read_bits(4), 0x4);
  EXPECT_EQ(buffer.read_bits(4), 0x3);
  EXPECT_EQ(buffer.read_bits(8), 0x12);
  EXPECT_EQ(buffer.read_bits(4), 0x8);
  EXPECT_EQ(buffer.read_bits(4), 0x7);
  EXPECT_EQ(buffer.read_bits(8), 0x56);
}

TEST(InputBufferTest, ReadBitsMoreThan16) {
  const uint8_t kTestData[] = {0x12, 0x34};
  auto          buffer      = TestInputBuffer(kTestData);

  expect_runtime_error(
      "number of bits to read: 17 is greater than max: 16", [&]() { buffer.read_bits(17); });
}

TEST(InputBufferTest, ReadWordOutOfRange) {
  const uint8_t kTestData[] = {0x12};
  auto          buffer      = TestInputBuffer(kTestData);

  expect_runtime_error("requested data out of range", [&]() { buffer.read_word(); });
}

#include "input_buffer.h"

#include <gtest/gtest.h>

#include <fstream>

// Helper function to create a temporary file with given content
std::string create_temp_file(const std::vector<uint8_t>& content) {
  std::string   filename = "/tmp/test_input_buffer.bin";
  std::ofstream file(filename, std::ios::binary);
  file.write(reinterpret_cast<const char*>(content.data()), content.size());
  file.close();
  return filename;
}

TEST(InputBufferTest, ReadWord) {
  std::vector<uint8_t> content  = {0x12, 0x34, 0x56, 0x78};
  std::string          filename = create_temp_file(content);

  auto buffer = InputBuffer::for_file(filename.c_str());
  ASSERT_NE(buffer, nullptr);

  EXPECT_EQ(buffer->read_word(), 0x1234);
  EXPECT_EQ(buffer->read_word(), 0x5678);
}

TEST(InputBufferTest, ReadBits) {
  std::vector<uint8_t> content  = {0x12, 0x34, 0x56, 0x78};
  std::string          filename = create_temp_file(content);

  auto buffer = InputBuffer::for_file(filename.c_str());
  ASSERT_NE(buffer, nullptr);

  EXPECT_EQ(buffer->read_bits(4), 0x4);
  EXPECT_EQ(buffer->read_bits(4), 0x3);
  EXPECT_EQ(buffer->read_bits(8), 0x12);
  EXPECT_EQ(buffer->read_bits(4), 0x8);
  EXPECT_EQ(buffer->read_bits(4), 0x7);
  EXPECT_EQ(buffer->read_bits(8), 0x56);
}

TEST(InputBufferTest, ReadBitsMoreThan16) {
  std::vector<uint8_t> content  = {0x12, 0x34};
  std::string          filename = create_temp_file(content);

  auto buffer = InputBuffer::for_file(filename.c_str());
  ASSERT_NE(buffer, nullptr);

  EXPECT_DEATH(buffer->read_bits(17), "Number of bits to read must be between 1 and 16");
}

TEST(InputBufferTest, ReadWordOutOfRange) {
  std::vector<uint8_t> content  = {0x12};
  std::string          filename = create_temp_file(content);

  auto buffer = InputBuffer::for_file(filename.c_str());
  ASSERT_NE(buffer, nullptr);

  EXPECT_DEATH(buffer->read_word(), "Read position out of range");
}

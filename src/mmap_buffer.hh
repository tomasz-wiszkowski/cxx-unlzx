#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "error.hh"

class InputBuffer {
 public:
  /**
   * @brief Default constructor for InputBuffer.
   */
  constexpr InputBuffer() = default;

  /**
   * @brief Constructs an InputBuffer over the given data array.
   * @param data Pointer to the memory data.
   * @param size Size of the memory data in bytes.
   */
  constexpr InputBuffer(const uint8_t* data, size_t size) : data_{data}, filesize_{size} {}

  /**
   * @brief Reads the requested number of bits into the output value.
   * @param data_bits_requested The number of bits to read.
   * @param out The variable where the read bits will be stored.
   * @return Status indicating success or the specific error encountered.
   */
  Status read_bits(size_t data_bits_requested, uint16_t& out);

  /**
   * @brief Peeks the requested number of bits without advancing the read position.
   * @param data_bits_requested The number of bits to peek.
   * @param out The variable where the peeked bits will be stored.
   * @return Status indicating success or the specific error encountered.
   */
  Status peek_bits(size_t data_bits_requested, uint16_t& out);

  /**
   * @brief Reads a specific length of bytes into the target memory.
   * @param target The target memory where data will be copied.
   * @param length The number of bytes to read.
   * @return Status indicating success or the specific error encountered.
   */
  Status read_into(void* target, size_t length);

  /**
   * @brief Reads a string view of the requested length.
   * @param length The length of the string to read.
   * @param out The string view pointing to the read string.
   * @return Status indicating success or the specific error encountered.
   */
  Status read_string_view(size_t length, std::string_view& out);

  /**
   * @brief Reads a span of the requested length.
   * @param length The length of the span to read.
   * @param out The span pointing to the read data.
   * @return Status indicating success or the specific error encountered.
   */
  Status read_span(size_t length, std::span<const uint8_t>& out);

  /**
   * @brief Reads a new buffer of the requested length from the current buffer.
   * @param length The length of the new buffer.
   * @param out The new InputBuffer containing the sliced data.
   * @return Status indicating success or the specific error encountered.
   */
  Status read_buffer(size_t length, InputBuffer& out);

  /**
   * @brief Skips a requested number of bytes and records the new position.
   * @param length The number of bytes to skip.
   * @param out_position The position after skipping.
   * @return Status indicating success or the specific error encountered.
   */
  Status skip(size_t length, size_t& out_position);

  /**
   * @brief Skips a requested number of bytes.
   * @param length The number of bytes to skip.
   * @return Status indicating success or the specific error encountered.
   */
  Status skip(size_t length);

  /**
   * @brief Gets the remaining available bytes in the buffer.
   * @return The number of bytes available.
   */
  size_t available() const {
    return filesize_ - current_position_;
  }

  /**
   * @brief Checks if the end of the buffer has been reached.
   * @return True if at the end of the buffer, false otherwise.
   */
  bool is_eof() const;

 protected:
  Status read_word(uint16_t& out);

 private:
  size_t         filesize_{};
  const uint8_t* data_{};
  size_t         current_position_{};

  uint32_t data_bits_{};
  size_t   data_bits_available_{};
};


class MmapInputBuffer {
 public:
  /**
   * @brief Creates a memory-mapped input buffer for the specified file.
   * @param filepath The path to the file to map.
   * @param out A unique pointer to the newly created MmapInputBuffer.
   * @return Status indicating success or the specific error encountered.
   */
  static Status for_file(const char* filepath, std::unique_ptr<MmapInputBuffer>& out);

  /**
   * @brief Destructor that unmaps the memory and closes the file handle.
   */
  ~MmapInputBuffer();

  /**
   * @brief Gets an InputBuffer representing the mapped file data.
   * @return The constructed InputBuffer.
   */
  InputBuffer get() const;

 private:
#ifdef _WIN32
  void*          file_handle_{};
  void*          mapping_handle_{};
#else
  int            fd_{};
#endif
  size_t         filesize_{};
  const uint8_t* data_{};
};
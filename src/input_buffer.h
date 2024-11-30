#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

class InputBuffer {
 public:
  static std::unique_ptr<InputBuffer> for_file(const char* filepath);
  ~InputBuffer();

  // To be deleted.
  uint8_t read_byte();
  // To be deleted.
  uint16_t read_word();

  uint16_t read_bits(size_t data_bits_requested);

  void                     read_into(void* target, size_t length);
  std::string_view         capture_as_string_view(size_t length);
  std::span<const uint8_t> capture_as_span(size_t length);
  size_t                   skip(size_t length);

  bool is_eof() const;

 private:
  int            fd_{};
  size_t         filesize_{};
  const uint8_t* data_{};
  size_t         current_position_{};

  size_t data_bits_{};
  size_t data_bits_available_{};
};
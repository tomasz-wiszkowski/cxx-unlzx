#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

class InputBuffer {
 public:
  constexpr InputBuffer(const uint8_t* data, size_t size) : data_{data}, filesize_{size} {}

  // To be deleted.
  uint8_t read_byte();
  // To be deleted.
  uint16_t read_word();

  uint16_t read_bits(size_t data_bits_requested);

  void                     read_into(void* target, size_t length);
  std::string_view         read_string_view(size_t length);
  std::span<const uint8_t> read_span(size_t length);
  InputBuffer              read_buffer(size_t length);
  size_t                   skip(size_t length);

  bool is_eof() const;

 private:
  size_t         filesize_{};
  const uint8_t* data_{};
  size_t         current_position_{};

  size_t data_bits_{};
  size_t data_bits_available_{};
};


class MmapInputBuffer {
 public:
  static std::unique_ptr<MmapInputBuffer> for_file(const char* filepath);
  ~MmapInputBuffer();

  InputBuffer get() const;

 private:
  int            fd_{};
  size_t         filesize_{};
  const uint8_t* data_{};
};
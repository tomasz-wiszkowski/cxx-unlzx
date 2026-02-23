#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "error.hh"

class InputBuffer {
 public:
  constexpr InputBuffer() = default;
  constexpr InputBuffer(const uint8_t* data, size_t size) : data_{data}, filesize_{size} {}

  Status read_bits(size_t data_bits_requested, uint16_t& out);
  Status peek_bits(size_t data_bits_requested, uint16_t& out);

  Status read_into(void* target, size_t length);
  Status read_string_view(size_t length, std::string_view& out);
  Status read_span(size_t length, std::span<const uint8_t>& out);
  Status read_buffer(size_t length, InputBuffer& out);
  Status skip(size_t length, size_t& out_position);
  Status skip(size_t length);
  size_t available() const {
    return filesize_ - current_position_;
  }

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
  static Status for_file(const char* filepath, std::unique_ptr<MmapInputBuffer>& out);
  ~MmapInputBuffer();

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
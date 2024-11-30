// Input buffer stream that uses mmap-ed file to retrieve content. Read-only.
// Assumes the file is composed of MSB uint16_t records.
// Allows the caller to read any arbitrary number of bits in a single call.
// Tracks current read position internally. Assumes sequential data access.
#include "input_buffer.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>

std::unique_ptr<MmapInputBuffer> MmapInputBuffer::for_file(const char* filepath) {
  int file_desc_ = open(filepath, O_RDONLY);
  if (file_desc_ == -1) return {};

  struct stat stat_buffer;
  if (fstat(file_desc_, &stat_buffer) == -1) {
    close(file_desc_);
    return {};
  }

  size_t filesize = stat_buffer.st_size;

  uint8_t* data =
      static_cast<uint8_t*>(mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, file_desc_, 0));
  if (data == MAP_FAILED) {
    close(file_desc_);
    throw std::runtime_error(std::format("unable to mmap input file \"{}\"", filepath));
  }

  auto res       = std::make_unique<MmapInputBuffer>();
  res->fd_       = file_desc_;
  res->data_     = data;
  res->filesize_ = filesize;

  return res;
}

MmapInputBuffer::~MmapInputBuffer() {
  munmap(const_cast<uint8_t*>(data_), filesize_);
  close(fd_);
}

InputBuffer MmapInputBuffer::get() const {
  return InputBuffer(data_, filesize_);
}

uint8_t InputBuffer::read_byte() {
  // less than 2 bytes available?
  if (filesize_ <= current_position_) {
    fputs("Read position out of range", stderr);
    std::exit(1);
  }

  return data_[current_position_++];
}

uint16_t InputBuffer::read_word() {
  // less than 2 bytes available?
  if (filesize_ < (current_position_ + 2)) {
    fputs("Read position out of range", stderr);
    std::exit(1);
  }

  uint16_t result = (data_[current_position_] << 8 | data_[current_position_ + 1]);
  current_position_ += 2;

  return result;
}

uint16_t InputBuffer::read_bits(size_t data_bits_requested) {
  if (data_bits_requested > 16 || data_bits_requested == 0) {
    fputs("Number of bits to read must be between 1 and 16", stderr);
    std::exit(1);
  }

  if (data_bits_available_ < data_bits_requested) {
    data_bits_ |= read_word() << data_bits_available_;
    data_bits_available_ += 16;
  }

  // Read N bits.
  // Turn data_bits_requested to a mask, e.g. "4" becomes "0b1111".
  uint16_t result = (data_bits_ & ((1 << data_bits_requested) - 1));
  // Consume bits.
  data_bits_ >>= data_bits_requested;
  data_bits_available_ -= data_bits_requested;

  return result;
}

void InputBuffer::read_into(void* target, size_t length) {
  size_t data_position = skip(length);
  std::memcpy(target, &data_[data_position], length);
}

bool InputBuffer::is_eof() const {
  return current_position_ == filesize_;
}

std::string_view InputBuffer::read_string_view(size_t length) {
  size_t data_position = skip(length);
  return std::string_view(reinterpret_cast<const char*>(&data_[data_position]), length);
}

std::span<const uint8_t> InputBuffer::read_span(size_t length) {
  size_t data_position = skip(length);
  return std::span<const uint8_t>(&data_[data_position], length);
}

InputBuffer InputBuffer::read_buffer(size_t length) {
  size_t data_position = skip(length);
  return InputBuffer(&data_[data_position], length);
}

size_t InputBuffer::skip(size_t length) {
  if (data_bits_available_ > 0) {
    throw std::runtime_error("cannot read misaligned data");
  }

  if (filesize_ < current_position_ + length) {
    throw std::runtime_error("unexpected end of input file");
  }

  size_t last_position = current_position_;
  current_position_ += length;

  return last_position;
}
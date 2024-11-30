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
#include <memory>

std::unique_ptr<InputBuffer> InputBuffer::for_file(const char* filepath) {
  int fd = open(filepath, O_RDONLY);
  if (fd == -1) return {};

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    close(fd);
    return {};
  }

  size_t filesize = sb.st_size;

  uint8_t* data = static_cast<uint8_t*>(mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, fd, 0));
  if (data == MAP_FAILED) {
    close(fd);
    return {};
  }

  auto res       = std::make_unique<InputBuffer>();
  res->fd_       = fd;
  res->data_     = data;
  res->filesize_ = filesize;

  return res;
}

InputBuffer::~InputBuffer() {
  munmap(data_, filesize_);
  close(fd_);
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

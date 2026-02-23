// Input buffer stream that uses mmap-ed file to retrieve content. Read-only.
// Assumes the file is composed of MSB uint16_t records.
// Allows the caller to read any arbitrary number of bits in a single call.
// Tracks current read position internally. Assumes sequential data access.
#include "mmap_buffer.hh"

#ifdef _WIN32
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>

#ifdef _WIN32

Status MmapInputBuffer::for_file(const char* filepath, std::unique_ptr<MmapInputBuffer>& out) {
  HANDLE file_handle =
      CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file_handle == INVALID_HANDLE_VALUE) return Status::FileOpenError;

  LARGE_INTEGER file_size;
  if (GetFileSizeEx(file_handle, &file_size) == FALSE) {
    CloseHandle(file_handle);
    return Status::FileOpenError;
  }

  size_t filesize = static_cast<size_t>(file_size.QuadPart);

  HANDLE mapping_handle =
      CreateFileMappingA(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (mapping_handle == nullptr) {
    CloseHandle(file_handle);
    return Status::FileMapError;
  }

  const uint8_t* data =
      static_cast<const uint8_t*>(MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0));
  if (data == nullptr) {
    CloseHandle(mapping_handle);
    CloseHandle(file_handle);
    return Status::FileMapError;
  }

  out                  = std::make_unique<MmapInputBuffer>();
  out->file_handle_    = file_handle;
  out->mapping_handle_ = mapping_handle;
  out->data_           = data;
  out->filesize_       = filesize;

  return Status::Ok;
}

MmapInputBuffer::~MmapInputBuffer() {
  UnmapViewOfFile(data_);
  CloseHandle(mapping_handle_);
  CloseHandle(file_handle_);
}

#else

Status MmapInputBuffer::for_file(const char* filepath, std::unique_ptr<MmapInputBuffer>& out) {
  int file_desc_ = open(filepath, O_RDONLY);
  if (file_desc_ == -1) return Status::FileOpenError;

  struct stat stat_buffer;
  if (fstat(file_desc_, &stat_buffer) == -1) {
    close(file_desc_);
    return Status::FileOpenError;
  }

  size_t filesize = stat_buffer.st_size;

  uint8_t* data =
      static_cast<uint8_t*>(mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, file_desc_, 0));
  if (data == MAP_FAILED) {
    close(file_desc_);
    return Status::FileMapError;
  }

  out           = std::make_unique<MmapInputBuffer>();
  out->fd_      = file_desc_;
  out->data_    = data;
  out->filesize_ = filesize;

  return Status::Ok;
}

MmapInputBuffer::~MmapInputBuffer() {
  munmap(const_cast<uint8_t*>(data_), filesize_);
  close(fd_);
}

#endif

InputBuffer MmapInputBuffer::get() const {
  return InputBuffer(data_, filesize_);
}

Status InputBuffer::read_word(uint16_t& out) {
  // less than 2 bytes available?
  if (filesize_ < (current_position_ + 2)) {
    return Status::OutOfRange;
  }

  out = (data_[current_position_] << 8) | (data_[current_position_ + 1]);
  current_position_ += 2;

  return Status::Ok;
}

Status InputBuffer::peek_bits(size_t data_bits_requested, uint16_t& out) {
  if (data_bits_requested > 16) {
    return Status::OutOfRange;
  }

  if (data_bits_available_ < data_bits_requested) {
    uint16_t word;
    TRY(read_word(word));
    data_bits_ |= word << data_bits_available_;
    data_bits_available_ += 16;
  }

  // Read N bits.
  // Turn data_bits_requested to a mask, e.g. "4" becomes "0b1111".
  out = (data_bits_ & ((1 << data_bits_requested) - 1));
  return Status::Ok;
}

Status InputBuffer::read_bits(size_t data_bits_requested, uint16_t& out) {
  TRY(peek_bits(data_bits_requested, out));
  // Consume bits.
  data_bits_ >>= data_bits_requested;
  data_bits_available_ -= data_bits_requested;

  return Status::Ok;
}

Status InputBuffer::read_into(void* target, size_t length) {
  size_t data_position;
  TRY(skip(length, data_position));
  std::memcpy(target, &data_[data_position], length);
  return Status::Ok;
}

bool InputBuffer::is_eof() const {
  return current_position_ == filesize_;
}

Status InputBuffer::read_string_view(size_t length, std::string_view& out) {
  size_t data_position;
  TRY(skip(length, data_position));
  out = std::string_view(reinterpret_cast<const char*>(&data_[data_position]), length);
  return Status::Ok;
}

Status InputBuffer::read_span(size_t length, std::span<const uint8_t>& out) {
  size_t data_position;
  TRY(skip(length, data_position));
  out = std::span<const uint8_t>(&data_[data_position], length);
  return Status::Ok;
}

Status InputBuffer::read_buffer(size_t length, InputBuffer& out) {
  size_t data_position;
  TRY(skip(length, data_position));
  out = InputBuffer(&data_[data_position], length);
  return Status::Ok;
}

Status InputBuffer::skip(size_t length, size_t& out_position) {
  if (data_bits_available_ > 0) {
    return Status::MisalignedData;
  }

  if (filesize_ < current_position_ + length) {
    return Status::UnexpectedEof;
  }

  out_position      = current_position_;
  current_position_ += length;

  return Status::Ok;
}

Status InputBuffer::skip(size_t length) {
  size_t unused;
  return skip(length, unused);
}
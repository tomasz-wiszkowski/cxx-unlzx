#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "error.hh"
#include "lzx_handle.hh"
#include "mmap_buffer.hh"

class LzxBlock {
public:
  /**
   * @brief Constructs an LzxBlock.
   * @param n The LZX entry metadata associated with this block.
   * @param in The input buffer containing the compressed data.
   * @param unpacked_sz The total expected size of the decompressed data.
   */
  LzxBlock(lzx::Entry n, InputBuffer in, size_t unpacked_sz)
      : node_(std::move(n)), in_file_(in), total_unpacked_size_(unpacked_sz) {}

  /**
   * @brief Decompresses the block data if not already done and returns a span to it.
   * @return An optional span containing the decompressed data.
   */
  std::optional<std::span<const uint8_t>> get_data();

  /**
   * @brief Gets the status of the last decompression operation.
   * @return The Status code.
   */
  Status get_status() const { return status_; }

  /**
   * @brief Gets the packed (compressed) size of the block.
   * @return The packed size in bytes.
   */
  size_t packed_size() const { return node_.pack_size(); }

  /**
   * @brief Gets the total unpacked (decompressed) size of the block.
   * @return The total unpacked size in bytes.
   */
  size_t total_unpacked_size() const { return total_unpacked_size_; }

private:
  lzx::Entry node_;
  InputBuffer in_file_;
  size_t total_unpacked_size_;

  std::optional<std::vector<uint8_t>> decompressed_data_;
  std::span<const uint8_t> data_span_;
  Status status_ = Status::Ok;
  bool is_decompressed_ = false;
};
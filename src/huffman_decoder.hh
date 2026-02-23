#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "error.hh"
#include "huffman_table.hh"
#include "mmap_buffer.hh"

namespace huffman {

class HuffmanDecoder {
 public:
  /**
   * @brief Constructs a new HuffmanDecoder.
   */
  HuffmanDecoder();

  /**
   * @brief Reads the literal table from the input buffer.
   * @param data The input buffer containing the literal table data.
   * @return Status indicating success or the specific error encountered.
   */
  Status read_literal_table(InputBuffer* data);

  /**
   * @brief Decrunches data from the input buffer into the target span.
   * @param data The input buffer containing compressed data.
   * @param target The target span to write decompressed data into.
   * @param pos The current position in the target span, updated upon successful decrunching.
   * @param threshold The threshold size for decrunching.
   * @return Status indicating success or the specific error encountered.
   */
  Status decrunch(InputBuffer* data, std::span<uint8_t> target, size_t& pos, size_t threshold);

  /**
   * @brief Gets the decrunched length.
   * @return The length of the decrunched data.
   */
  uint32_t decrunch_length() const {
    return decrunch_length_;
  }

 private:
  HuffmanTable offsets_;
  HuffmanTable huffman20_;
  HuffmanTable literals_;

  uint32_t decrunch_method_{};
  uint32_t decrunch_length_{};

  uint32_t last_offset_{1};
};

}  // namespace huffman
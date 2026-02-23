#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "circular_buffer.hh"
#include "error.hh"
#include "huffman_table.hh"
#include "mmap_buffer.hh"

namespace huffman {

class HuffmanDecoder {
 public:
  HuffmanDecoder();
  Status read_literal_table(InputBuffer* data);
  Status decrunch(InputBuffer* data, std::span<uint8_t> target, size_t& pos, size_t threshold);

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
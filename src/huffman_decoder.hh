#pragma once

#include <cstdint>

#include "circular_buffer.hh"
#include "huffman_table.hh"
#include "mmap_buffer.hh"

namespace huffman {

class HuffmanDecoder {
 public:
  HuffmanDecoder();
  void read_literal_table(InputBuffer* data);
  void decrunch(InputBuffer* data, CircularBuffer<uint8_t>* buffer, size_t max_decode_length);

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
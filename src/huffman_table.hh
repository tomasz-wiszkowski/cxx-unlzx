#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace huffman {
class HuffmanTable {
 public:
  HuffmanTable(size_t table_bits, size_t num_symbols, size_t num_decode_entries);
  // Num bits per entry.
  const size_t table_bits_;
  // Array of bit lengths for each symbol.
  std::vector<uint8_t> bit_length_;
  // The decode table.
  std::vector<uint16_t> table_;

  void reset_table();
};

}  // namespace huffman
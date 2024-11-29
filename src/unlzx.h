#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

enum class Action { View, Extract };

int process_archive(char* filename, Action action);

namespace crc {
void     reset();
uint32_t calc(uint8_t* memory, size_t length);
uint32_t sum();
}  // namespace crc

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

  bool reset_table();
};

}  // namespace huffman
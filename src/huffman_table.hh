#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "error.hh"

namespace huffman {
class HuffmanTable {
 public:
  /**
   * @brief Constructs a new HuffmanTable.
   * @param table_bits Number of bits per entry in the table.
   * @param num_symbols Number of symbols.
   * @param num_decode_entries Number of decode entries.
   */
  HuffmanTable(size_t table_bits, size_t num_symbols, size_t num_decode_entries);
  // Num bits per entry.
  const size_t table_bits_;
  // Array of bit lengths for each symbol.
  std::vector<uint8_t> bit_length_;
  // The decode table.
  std::vector<uint16_t> table_;

  /**
   * @brief Resets the Huffman table.
   * @return Status indicating success or the specific error encountered.
   */
  Status reset_table();
};

}  // namespace huffman
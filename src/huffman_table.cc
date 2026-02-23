#include "huffman_table.hh"

#include <cstdint>
#include <stdexcept>

namespace huffman {

HuffmanTable::HuffmanTable(size_t table_bits, size_t num_symbols, size_t num_decode_entries)
    : table_bits_{table_bits} {
  bit_length_.resize(num_symbols);
  table_.resize(num_decode_entries);
}

/**
 * @brief Build a fast Huffman decode table from the symbol bit lengths.
 *
 * This function constructs a decode table for Huffman coding based on the provided bit lengths
 * for each symbol. It handles symbols with bit lengths both less than or equal to, and greater
 * than the specified table size.
 *
 * @param table_bits The number of bits in the decode table.
 * @return bool Returns true on success, false if an error occurred (e.g., table creation was
 * aborted).
 */
// NOLINTBEGIN(readability-function-cognitive-complexity)
Status HuffmanTable::reset_table() {
  uint8_t  current_bit_length = 0;
  uint32_t symbol;
  uint32_t leaf;
  uint32_t table_mask = 1 << table_bits_;
  uint32_t bit_mask   = table_mask >> 1;
  uint32_t position   = 0;
  uint32_t fill;
  uint32_t next_symbol;
  uint32_t reversed_position;

  current_bit_length++;

  // Reverse the position bits, keeping at most `table_bits_`.
  // e.g. if `value` is 0bABCD and `table_bits_` is 3, then result will be 0bDCB.
  auto reverse_bits = [this](uint32_t value) {
    uint32_t leaf = 0;
    for (int i = 0; i < table_bits_; i++) {
      leaf = (leaf << 1) | ((value >> i) & 1);
    }
    return leaf;
  };

  // First pass: fill the table for symbols with bit lengths <= table_bits_
  while (current_bit_length <= table_bits_) {
    for (symbol = 0; symbol < bit_length_.size(); symbol++) {
      if (bit_length_[symbol] != current_bit_length) {
        continue;
      }

      leaf = reverse_bits(position);

      position += bit_mask;
      if (position > table_mask) {
        return Status::HuffmanTableError;
      }

      fill        = bit_mask;
      next_symbol = 1 << current_bit_length;
      while (fill > 0) {
        fill--;
        table_[leaf] = symbol;
        leaf += next_symbol;
      }
    }
    bit_mask >>= 1;
    current_bit_length++;
  }

  // Second pass: fill the table for symbols with bit lengths > table_bits_
  if (position != table_mask) {
    for (symbol = position; symbol < table_mask; symbol++) {
      leaf         = reverse_bits(symbol);
      table_[leaf] = 0;
    }

    next_symbol = table_mask >> 1;
    position <<= 16;
    table_mask <<= 16;
    bit_mask = 32768;

    while (current_bit_length <= 16) {
      for (symbol = 0; symbol < bit_length_.size(); symbol++) {
        if (bit_length_[symbol] == current_bit_length) {
          leaf = reverse_bits(position >> 16);

          for (fill = 0; fill < current_bit_length - table_bits_; fill++) {
            if (table_[leaf] == 0) {
              table_[next_symbol << 1]       = 0;
              table_[(next_symbol << 1) + 1] = 0;
              table_[leaf]                   = next_symbol++;
            }
            leaf = (table_[leaf] << 1) | ((position >> (15 - fill)) & 1);
          }

          table_[leaf] = symbol;
          position += bit_mask;
          if (position > table_mask) {
            return Status::HuffmanTableError;
          }
        }
      }
      bit_mask >>= 1;
      current_bit_length++;
    }
  }

  if (position != table_mask) {
    return Status::HuffmanTableError;
  }

  return Status::Ok;
}
// NOLINTEND(readability-function-cognitive-complexity)

}  // namespace huffman
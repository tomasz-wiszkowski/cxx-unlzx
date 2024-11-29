#include "unlzx.h"

#include <cstdint>

namespace huffman {

/**
 * @brief Build a fast Huffman decode table from the symbol bit lengths.
 * 
 * This function constructs a decode table for Huffman coding based on the provided bit lengths
 * for each symbol. It handles symbols with bit lengths both less than or equal to, and greater
 * than the specified table size.
 * 
 * @param num_symbols The number of symbols to decode.
 * @param table_bits The number of bits in the decode table.
 * @param bit_lengths Array of bit lengths for each symbol.
 * @param decode_table The decode table to be filled.
 * @return bool Returns true on success, false if an error occurred (e.g., table creation was aborted).
 */
bool make_decode_table(
    int num_symbols, int table_bits, uint8_t* bit_lengths, uint16_t* decode_table) {
    uint8_t current_bit_length = 0;
    int symbol;
    uint32_t leaf;
    uint32_t table_mask = 1 << table_bits;
    uint32_t bit_mask = table_mask >> 1;
    uint32_t position = 0;
    uint32_t fill, next_symbol, reversed_position;

    current_bit_length++;

    // First pass: fill the table for symbols with bit lengths <= table_bits
    while (current_bit_length <= table_bits) {
        for (symbol = 0; symbol < num_symbols; symbol++) {
            if (bit_lengths[symbol] == current_bit_length) {
                reversed_position = position;
                leaf = 0;
                fill = table_bits;

                // Reverse the position bits
                while (fill--) {
                    leaf = (leaf << 1) | (reversed_position & 1);
                    reversed_position >>= 1;
                }

                position += bit_mask;
                if (position > table_mask) {
                    return false; // Abort due to position exceeding table mask
                }

                fill = bit_mask;
                next_symbol = 1 << current_bit_length;
                while (fill--) {
                    decode_table[leaf] = symbol;
                    leaf += next_symbol;
                }
            }
        }
        bit_mask >>= 1;
        current_bit_length++;
    }

    // Second pass: fill the table for symbols with bit lengths > table_bits
    if (position != table_mask) {
        for (symbol = position; symbol < table_mask; symbol++) {
            reversed_position = symbol;
            leaf = 0;
            fill = table_bits;

            // Reverse the position bits
            while (fill--) {
                leaf = (leaf << 1) | (reversed_position & 1);
                reversed_position >>= 1;
            }

            decode_table[leaf] = 0;
        }

        next_symbol = table_mask >> 1;
        position <<= 16;
        table_mask <<= 16;
        bit_mask = 32768;

        while (current_bit_length <= 16) {
            for (symbol = 0; symbol < num_symbols; symbol++) {
                if (bit_lengths[symbol] == current_bit_length) {
                    reversed_position = position >> 16;
                    leaf = 0;
                    fill = table_bits;

                    // Reverse the position bits
                    while (fill--) {
                        leaf = (leaf << 1) | (reversed_position & 1);
                        reversed_position >>= 1;
                    }

                    for (fill = 0; fill < current_bit_length - table_bits; fill++) {
                        if (decode_table[leaf] == 0) {
                            decode_table[next_symbol << 1] = 0;
                            decode_table[(next_symbol << 1) + 1] = 0;
                            decode_table[leaf] = next_symbol++;
                        }
                        leaf = (decode_table[leaf] << 1) | ((position >> (15 - fill)) & 1);
                    }

                    decode_table[leaf] = symbol;
                    position += bit_mask;
                    if (position > table_mask) {
                        return false; // Abort due to position exceeding table mask
                    }
                }
            }
            bit_mask >>= 1;
            current_bit_length++;
        }
    }

    return position == table_mask;
}

}  // namespace huffman
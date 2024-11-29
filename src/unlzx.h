#pragma once

#include <stdint.h>
#include <stddef.h>

enum class Action {
    View,
    Extract
};

int process_archive(char *filename, Action action);

namespace crc {
void reset();
uint32_t calc(uint8_t *memory, size_t length);
uint32_t sum();
}

namespace huffman {
bool make_decode_table(int num_symbols, int table_bits, uint8_t *bit_lengths, uint16_t *decode_table);
}
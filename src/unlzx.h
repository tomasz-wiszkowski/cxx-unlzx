#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

extern uint8_t  decrunch_buffer[258 + 65536 + 258]; /* allow overrun for speed */
extern uint8_t* source;
extern uint8_t* destination;
extern uint8_t* source_end;
extern uint8_t* destination_end;

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

class HuffmanDecoder {
 public:
  HuffmanDecoder();
  int  read_literal_table();
  void decrunch();

  uint32_t decrunch_length() const {
    return decrunch_length_;
  }

 private:
  HuffmanTable offsets_;
  HuffmanTable huffman20_;
  HuffmanTable literals_;

  uint32_t global_control_{};
  int32_t  global_shift_{-16};
  uint32_t decrunch_method_{};
  uint32_t decrunch_length_{};

  uint32_t last_offset_{1};
};
}  // namespace huffman
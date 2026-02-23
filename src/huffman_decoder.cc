#include "huffman_decoder.hh"

#include <algorithm>
#include <cstdint>

namespace huffman {
namespace {
constexpr uint8_t kSymbolBitLengths[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
    8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

constexpr uint32_t kBaseOffsets[32] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192,
    256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768,
    49152};

constexpr uint8_t kBaseValues[34] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0, 1,
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

constexpr uint8_t kSymbolZeroFill          = 17;
constexpr uint8_t kSymbolRepeatZero        = 18;
constexpr uint8_t kSymbolRepeatPrevious    = 19;
constexpr uint8_t kSymbolLongerThanSixBits = 20;

}  // namespace

HuffmanDecoder::HuffmanDecoder()
    : offsets_(7, 8, 128), huffman20_(6, 20, 96), literals_(12, 768, 5120) {}

Status HuffmanDecoder::read_literal_table(InputBuffer* source) {
  uint32_t symbol;
  uint32_t pos;
  uint32_t count;
  uint32_t fix;
  uint32_t max_symbol;

  // Read the decrunch method
  uint16_t method;
  TRY(source->read_bits(3, method));
  decrunch_method_ = method;

  // Read and build the offset Huffman table
  if (decrunch_method_ == 3) {
    for (auto& bl : offsets_.bit_length_) {
      uint16_t val;
      TRY(source->read_bits(3, val));
      bl = static_cast<uint8_t>(val);
    }
    TRY(offsets_.reset_table());
  }

  // Read decrunch length
  uint16_t b1, b2, b3;
  TRY(source->read_bits(8, b1));
  TRY(source->read_bits(8, b2));
  TRY(source->read_bits(8, b3));
  decrunch_length_ = (b1 << 16) | (b2 << 8) | b3;

  auto fill_literals_bit_lengths = [&](uint32_t byte_count, uint8_t value) {
    uint32_t fill_length = std::min(byte_count, max_symbol - pos);
    std::fill_n(literals_.bit_length_.begin() + pos, fill_length, value);
    return fill_length;
  };

  // Read and build the Huffman literal table
  if (decrunch_method_ != 1) {
    pos        = 0;
    fix        = 1;
    max_symbol = 256;

    do {
      for (auto& bl : huffman20_.bit_length_) {
        uint16_t val;
        TRY(source->read_bits(4, val));
        bl = static_cast<uint8_t>(val);
      }
      TRY(huffman20_.reset_table());

      do {
        uint16_t peeked;
        TRY(source->peek_bits(6, peeked));
        symbol = huffman20_.table_[peeked];

        if (symbol >= kSymbolLongerThanSixBits) {
          source->skip(0);  // This is not quite right if we need to consume bits.
          // peek_bits/read_bits handle bit consumption.
          // source->read_bits(6, peeked) consumes them.
          TRY(source->read_bits(6, peeked));

          do {
            uint16_t bit;
            TRY(source->read_bits(1, bit));
            symbol = huffman20_.table_[symbol << 1 | bit];
          } while (symbol >= kSymbolLongerThanSixBits);
        } else {
          uint16_t unused;
          TRY(source->read_bits(huffman20_.bit_length_[symbol], unused));
        }


        switch (symbol) {
        case kSymbolZeroFill: {
          uint16_t bits;
          TRY(source->read_bits(4, bits));
          count = 3 + bits + fix;
          pos += fill_literals_bit_lengths(count, 0);
          break;
        }

        case kSymbolRepeatZero: {
          uint16_t bits;
          TRY(source->read_bits(6 - fix, bits));
          count = 19 + bits + fix;
          pos += fill_literals_bit_lengths(count, 0);
          break;
        }


        case kSymbolRepeatPrevious: {
          uint16_t bits;
          TRY(source->read_bits(1, bits));
          count = 3 + bits + fix;

          TRY(source->peek_bits(6, bits));
          symbol = huffman20_.table_[bits];

          if (symbol >= kSymbolLongerThanSixBits) {
            uint16_t unused;
            TRY(source->read_bits(6, unused));
            do {  // Symbol is longer than 6 bits
              TRY(source->read_bits(1, bits));
              symbol = huffman20_.table_[(symbol << 1) | bits];
            } while (symbol >= kSymbolLongerThanSixBits);
          } else {
            uint16_t unused;
            TRY(source->read_bits(huffman20_.bit_length_[symbol], unused));
          }

          symbol = kBaseValues[literals_.bit_length_[pos] + 17 - symbol];
          pos += fill_literals_bit_lengths(count, symbol);
          break;
        }

        default:
          literals_.bit_length_[pos++] = kBaseValues[literals_.bit_length_[pos] + 17 - symbol];
          break;
        }
      } while (pos < max_symbol);
      fix--;
      max_symbol += 512;
    } while (max_symbol == 768);

    TRY(literals_.reset_table());
  }
  return Status::Ok;
}

/* ---------------------------------------------------------------------- */

/* Fill up the decrunch buffer. Needs lots of overrun for both destination */
/* and source buffers. Most of the time is spent in this routine so it's  */
/* pretty damn optimized. */

Status HuffmanDecoder::decrunch(
    InputBuffer* source, std::span<uint8_t> target, size_t& pos, size_t threshold) {
  uint32_t temp;
  uint32_t symbol;
  uint32_t count;

  while (pos < threshold && (!source->is_eof())) {
    uint16_t symbol_data;
    TRY(source->peek_bits(12, symbol_data));
    symbol = literals_.table_[symbol_data];

    if (symbol >= 768) {
      uint16_t unused;
      TRY(source->read_bits(12, unused));

      do { /* literal is longer than 12 bits */
        uint16_t bit;
        TRY(source->read_bits(1, bit));
        symbol = literals_.table_[(symbol << 1) | bit];
      } while (symbol >= 768);
    } else {
      temp = literals_.bit_length_[symbol];
      uint16_t unused;
      TRY(source->read_bits(temp, unused));
    }

    // Direct byte to put in the decode buffer.
    if (symbol < 256) {
      if (pos >= target.size()) return Status::BufferOverflow;
      target[pos++] = static_cast<uint8_t>(symbol);
      continue;
    }

    symbol -= 256;
    temp  = symbol & 31;
    count = kBaseOffsets[temp];
    temp  = kSymbolBitLengths[temp];
    if ((temp >= 3) && (decrunch_method_ == 3)) {
      temp -= 3;
      uint16_t bits;
      TRY(source->read_bits(temp, bits));
      count += bits << 3;

      TRY(source->peek_bits(7, bits));
      count += (temp = offsets_.table_[bits]);
      temp = offsets_.bit_length_[temp];
    } else {
      uint16_t bits;
      TRY(source->peek_bits(temp, bits));
      count += bits;
      if (count == 0U) count = last_offset_;
    }

    uint16_t unused;
    TRY(source->read_bits(temp, unused));
    last_offset_ = count;

    count = kBaseOffsets[temp = (symbol >> 5) & 15] + 3;
    temp  = kSymbolBitLengths[temp];
    uint16_t bits;
    TRY(source->read_bits(temp, bits));
    count += bits;

    if (last_offset_ > pos) return Status::OutOfRange;
    if (pos + count > target.size()) return Status::BufferOverflow;

    size_t start_idx = pos - last_offset_;
    for (size_t i = 0; i < count; ++i) {
      target[pos++] = target[start_idx + i];
    }
  }
  return Status::Ok;
}

}  // namespace huffman

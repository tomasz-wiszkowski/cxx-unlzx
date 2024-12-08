#include <algorithm>
#include <cstdint>

#include "unlzx.hh"

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

void HuffmanDecoder::read_literal_table(InputBuffer* source) {
  uint32_t symbol;
  uint32_t pos;
  uint32_t count;
  uint32_t fix;
  uint32_t max_symbol;

  // Read the decrunch method
  decrunch_method_ = source->read_bits(3);

  // Read and build the offset Huffman table
  if (decrunch_method_ == 3) {
    std::generate(offsets_.bit_length_.begin(), offsets_.bit_length_.end(),
        [&source]() { return source->read_bits(3); });
    offsets_.reset_table();
  }

  // Read decrunch length
  decrunch_length_ = source->read_bits(8) << 16;
  decrunch_length_ |= source->read_bits(8) << 8;
  decrunch_length_ |= source->read_bits(8);

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
      std::generate(huffman20_.bit_length_.begin(), huffman20_.bit_length_.end(),
          [&source]() { return source->read_bits(4); });
      huffman20_.reset_table();

      do {
        symbol = huffman20_.table_[source->peek_bits(6)];
        if (symbol >= kSymbolLongerThanSixBits) {
          source->read_bits(6);

          do {
            symbol = huffman20_.table_[symbol << 1 | source->read_bits(1)];
          } while (symbol >= kSymbolLongerThanSixBits);
        } else {
          source->read_bits(huffman20_.bit_length_[symbol]);
        }


        switch (symbol) {
        case kSymbolZeroFill:
          count = 3 + source->read_bits(4) + fix;
          pos += fill_literals_bit_lengths(count, 0);
          break;

        case kSymbolRepeatZero:
          count = 19 + source->read_bits(6 - fix) + fix;
          pos += fill_literals_bit_lengths(count, 0);
          break;


        case kSymbolRepeatPrevious: {
          count  = 3 + source->read_bits(1) + fix;
          symbol = huffman20_.table_[source->peek_bits(6)];

          if (symbol >= kSymbolLongerThanSixBits) {
            source->read_bits(6);
            do {  // Symbol is longer than 6 bits
              symbol = huffman20_.table_[(symbol << 1) | source->read_bits(1)];
            } while (symbol >= kSymbolLongerThanSixBits);
          } else {
            source->read_bits(huffman20_.bit_length_[symbol]);
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

    literals_.reset_table();
  }
}

/* ---------------------------------------------------------------------- */

/* Fill up the decrunch buffer. Needs lots of overrun for both destination */
/* and source buffers. Most of the time is spent in this routine so it's  */
/* pretty damn optimized. */

void HuffmanDecoder::decrunch(
    InputBuffer* source, CircularBuffer<uint8_t>* target, size_t threshold) {
  uint32_t temp;
  uint32_t symbol;
  uint32_t count;

  while (target->size() < threshold && (!source->is_eof())) {
    uint32_t symbol_data = source->peek_bits(12);
    symbol               = literals_.table_[symbol_data];

    if (symbol >= 768) {
      source->read_bits(12);

      do { /* literal is longer than 12 bits */
        symbol = literals_.table_[(symbol << 1) | source->read_bits(1)];
      } while (symbol >= 768);
    } else {
      temp = literals_.bit_length_[symbol];
      source->read_bits(temp);
    }

    // Direct byte to put in the decode buffer.
    if (symbol < 256) {
      target->push(symbol);
      continue;
    }

    symbol -= 256;
    temp  = symbol & 31;
    count = kBaseOffsets[temp];
    temp  = kSymbolBitLengths[temp];
    if ((temp >= 3) && (decrunch_method_ == 3)) {
      temp -= 3;
      count += source->read_bits(temp) << 3;
      count += (temp = offsets_.table_[source->peek_bits(7)]);
      temp = offsets_.bit_length_[temp];
    } else {
      count += source->peek_bits(temp);
      if (count == 0U) count = last_offset_;
    }

    source->read_bits(temp);
    last_offset_ = count;

    count = kBaseOffsets[temp = (symbol >> 5) & 15] + 3;
    temp  = kSymbolBitLengths[temp];
    count += source->read_bits(temp);

    target->repeat(last_offset_, count);
  }
}

}  // namespace huffman

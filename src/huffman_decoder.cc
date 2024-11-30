#include <cstdint>

#include "unlzx.h"

namespace huffman {
namespace {
constexpr uint8_t kSymbolBitLengths[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
    8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

constexpr uint32_t kBaseOffsets[32] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192,
    256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768,
    49152};

constexpr uint32_t kBitLengthMasks[16] = {
    0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767};

constexpr uint8_t kBaseValues[34] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0, 1,
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

constexpr uint8_t kSymbolZeroFill          = 17;
constexpr uint8_t kSymbolRepeatZero        = 18;
constexpr uint8_t kSymbolRepeatPrevious    = 19;
constexpr uint8_t kSymbolLongerThanSixBits = 20;

}  // namespace

HuffmanDecoder::HuffmanDecoder()
    : offsets_(7, 8, 128), huffman20_(6, 20, 96), literals_(12, 768, 5120) {}

int HuffmanDecoder::read_literal_table(InputBuffer* source) {
  uint32_t control = global_control_;
  int32_t  shift   = global_shift_;
  uint32_t temp;
  uint32_t symbol;
  uint32_t pos;
  uint32_t count;
  uint32_t fix;
  uint32_t max_symbol;

  // Fix the control word if necessary
  if (shift <= 0) {
    shift += 16;
    control += source->read_word() << shift;
  }

  // Read the decrunch method
  decrunch_method_ = control & 7;
  control >>= 3;
  shift -= 3;
  if (shift <= 0) {
    shift += 16;
    control += source->read_word() << shift;
  }

  // Read and build the offset Huffman table
  if (decrunch_method_ == 3) {
    for (temp = 0; temp < 8; temp++) {
      offsets_.bit_length_[temp] = control & 7;
      control >>= 3;
      shift -= 3;
      if (shift <= 0) {
        shift += 16;
        control += source->read_word() << shift;
      }
    }
    if (!offsets_.reset_table()) {
      return 1;  // Failure in building offset Huffman table
    }
  }

  // Read decrunch length
  decrunch_length_ = (control & 255) << 16;
  control >>= 8;
  shift -= 8;
  if (shift <= 0) {
    shift += 16;
    control += source->read_word() << shift;
  }
  decrunch_length_ += (control & 255) << 8;
  control >>= 8;
  shift -= 8;
  if (shift <= 0) {
    shift += 16;
    control += source->read_word() << shift;
  }
  decrunch_length_ += (control & 255);
  control >>= 8;
  shift -= 8;
  if (shift <= 0) {
    shift += 16;
    control += source->read_word() << shift;
  }

  // Read and build the Huffman literal table
  if (decrunch_method_ != 1) {
    pos        = 0;
    fix        = 1;
    max_symbol = 256;

    do {
      for (temp = 0; temp < 20; temp++) {
        huffman20_.bit_length_[temp] = control & 15;
        control >>= 4;
        shift -= 4;
        if (shift <= 0) {
          shift += 16;
          control += source->read_word() << shift;
        }
      }
      if (!huffman20_.reset_table()) {
        return 1;  // Failure in building Huffman 20 table
      }

      do {
        symbol = huffman20_.table_[control & 63];
        if (symbol >= kSymbolLongerThanSixBits) {
          do {  // Symbol is longer than 6 bits
            symbol = huffman20_.table_[((control >> 6) & 1) + (symbol << 1)];
            shift--;
            if (shift < 0) {
              shift += 16;
              control += source->read_word() << 16;
            }
            control >>= 1;
          } while (symbol >= kSymbolLongerThanSixBits);
          temp = 6;
        } else {
          temp = huffman20_.bit_length_[symbol];
        }
        control >>= temp;
        shift -= temp;
        if (shift <= 0) {
          shift += 16;
          control += source->read_word() << shift;
        }
        switch (symbol) {

        case kSymbolZeroFill:
        case kSymbolRepeatZero:
          if (symbol == kSymbolZeroFill) {
            temp  = 4;
            count = 3;
          } else {  // symbol == kSymbolRepeatZero
            temp  = 6 - fix;
            count = 19;
          }
          count += (control & kBitLengthMasks[temp]) + fix;
          control >>= temp;
          shift -= temp;
          if (shift <= 0) {
            shift += 16;
            control += source->read_word() << shift;
          }
          while (pos < max_symbol && ((count--) != 0U)) {
            literals_.bit_length_[pos++] = 0;
          }
          break;


        case kSymbolRepeatPrevious:
          count = (control & 1) + 3 + fix;
          shift--;
          if (shift < 0) {
            shift += 16;
            control += source->read_word() << 16;
          }
          control >>= 1;
          symbol = huffman20_.table_[control & 63];
          if (symbol >= kSymbolLongerThanSixBits) {
            do {  // Symbol is longer than 6 bits
              symbol = huffman20_.table_[((control >> 6) & 1) + (symbol << 1)];
              shift--;
              if (shift < 0) {
                shift += 16;
                control += source->read_word() << 16;
              }
              control >>= 1;
            } while (symbol >= kSymbolLongerThanSixBits);
            temp = 6;
          } else {
            temp = huffman20_.bit_length_[symbol];
          }
          control >>= temp;
          shift -= temp;
          if (shift <= 0) {
            shift += 16;
            control += source->read_word() << shift;
          }
          symbol = kBaseValues[literals_.bit_length_[pos] + 17 - symbol];
          while (pos < max_symbol && ((count--) != 0U)) {
            literals_.bit_length_[pos++] = symbol;
          }
          break;


        default:
          symbol                       = kBaseValues[literals_.bit_length_[pos] + 17 - symbol];
          literals_.bit_length_[pos++] = symbol;
          break;
        }
      } while (pos < max_symbol);
      fix--;
      max_symbol += 512;
    } while (max_symbol == 768);

    if (!literals_.reset_table()) {
      return 1;  // Failure in building literal table
    }
  }

  global_control_ = control;
  global_shift_   = shift;

  return 0;  // Success
}

/* ---------------------------------------------------------------------- */

/* Fill up the decrunch buffer. Needs lots of overrun for both destination */
/* and source buffers. Most of the time is spent in this routine so it's  */
/* pretty damn optimized. */

void HuffmanDecoder::decrunch(InputBuffer* source) {
  unsigned int   control;
  int            shift;
  unsigned int   temp; /* could be a register */
  unsigned int   symbol;
  unsigned int   count;
  unsigned char* string;

  control = global_control_;
  shift   = global_shift_;

  do {
    if ((symbol = literals_.table_[control & 4095]) >= 768) {
      control >>= 12;
      if ((shift -= 12) < 0) {
        shift += 16;
        control += source->read_word() << shift;
      }
      do { /* literal is longer than 12 bits */
        symbol = literals_.table_[(control & 1) + (symbol << 1)];
        if ((shift--) == 0) {
          shift += 16;
          control += source->read_word() << 16;
        }
        control >>= 1;
      } while (symbol >= 768);
    } else {
      temp = literals_.bit_length_[symbol];
      control >>= temp;
      if ((shift -= temp) < 0) {
        shift += 16;
        control += source->read_word() << shift;
      }
    }
    if (symbol < 256) {
      *destination++ = symbol;
    } else {
      symbol -= 256;
      count = kBaseOffsets[temp = symbol & 31];
      temp  = kSymbolBitLengths[temp];
      if ((temp >= 3) && (decrunch_method_ == 3)) {
        temp -= 3;
        count += ((control & kBitLengthMasks[temp]) << 3);
        control >>= temp;
        if ((shift -= temp) < 0) {
          shift += 16;
          control += source->read_word() << shift;
        }
        count += (temp = offsets_.table_[control & 127]);
        temp = offsets_.bit_length_[temp];
      } else {
        count += control & kBitLengthMasks[temp];
        if (count == 0U) count = last_offset_;
      }
      control >>= temp;
      if ((shift -= temp) < 0) {
        shift += 16;
        control += source->read_word() << shift;
      }
      last_offset_ = count;

      count = kBaseOffsets[temp = (symbol >> 5) & 15] + 3;
      temp  = kSymbolBitLengths[temp];
      count += (control & kBitLengthMasks[temp]);
      control >>= temp;
      if ((shift -= temp) < 0) {
        shift += 16;
        control += source->read_word() << shift;
      }
      string = (decrunch_buffer + last_offset_ < destination) ? destination - last_offset_
                                                              : destination + 65536 - last_offset_;
      do {
        *destination++ = *string++;
      } while (--count != 0U);
    }
  } while ((destination < destination_end) && (!source->is_eof()));

  global_control_ = control;
  global_shift_   = shift;
}

}  // namespace huffman
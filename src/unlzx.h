#pragma once

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <memory>
#include <string>
#include <vector>

#include "input_buffer.h"

extern uint8_t        decrunch_buffer[258 + 65536 + 258]; /* allow overrun for speed */
extern const uint8_t* source;
extern const uint8_t* source_end;
extern uint8_t*       destination;
extern uint8_t*       destination_end;

enum class Action : uint8_t { View, Extract };

void process_archive(char* filename, Action action);

constexpr static uint32_t big_data_to_host(uint32_t data) {
  if constexpr (std::endian::native == std::endian::big) {
    return data;
  } else {
    return std::byteswap(data);
  }
}

constexpr static uint32_t data_to_host(uint32_t data) {
  if constexpr (std::endian::native == std::endian::little) {
    return data;
  } else {
    return std::byteswap(data);
  }
}


class ArchivedDateStamp {
  uint32_t stamp_;

  static constexpr uint32_t kShiftSeconds = 0;
  static constexpr uint32_t kBitsSeconds  = 6;
  static constexpr uint32_t kMaskSeconds  = (1 << kBitsSeconds) - 1;

  static constexpr uint32_t kShiftMinutes = kShiftSeconds + kBitsSeconds;
  static constexpr uint32_t kBitsMinutes  = 6;
  static constexpr uint32_t kMaskMinutes  = (1 << kBitsMinutes) - 1;

  static constexpr uint32_t kShiftHours = kShiftMinutes + kBitsMinutes;
  static constexpr uint32_t kBitsHours  = 5;
  static constexpr uint32_t kMaskHours  = (1 << kBitsHours) - 1;

  static constexpr uint32_t kShiftYear = kShiftHours + kBitsHours;
  static constexpr uint32_t kBitsYear  = 6;
  static constexpr uint32_t kMaskYear  = (1 << kBitsYear) - 1;

  static constexpr uint32_t kShiftMonth = kShiftYear + kBitsYear;
  static constexpr uint32_t kBitsMonth  = 4;
  static constexpr uint32_t kMaskMonth  = (1 << kBitsMonth) - 1;

  static constexpr uint32_t kShiftDay = kShiftMonth + kBitsMonth;
  static constexpr uint32_t kBitsDay  = 5;
  static constexpr uint32_t kMaskDay  = (1 << kBitsDay) - 1;

  static_assert(kShiftDay + kBitsDay == 32);

 public:
  constexpr uint32_t raw() const {
    return big_data_to_host(stamp_);
  }

  constexpr uint32_t year() const {
    return ((raw() >> kShiftYear) & kMaskYear) + 1970;
  }

  constexpr uint32_t month() const {
    return (raw() >> kShiftMonth) & kMaskMonth;
  }

  constexpr uint32_t day() const {
    return (raw() >> kShiftDay) & kMaskDay;
  }

  constexpr uint32_t hour() const {
    return (raw() >> kShiftHours) & kMaskHours;
  }

  constexpr uint32_t minute() const {
    return (raw() >> kShiftMinutes) & kMaskMinutes;
  }

  constexpr uint32_t second() const {
    return (raw() >> kShiftSeconds) & kMaskSeconds;
  }
} __attribute__((packed));

static_assert(sizeof(ArchivedDateStamp) == 4);

class ArchivedPackMode {
  uint8_t type_;

 public:
  static constexpr const uint8_t kCompressionNone   = 0;
  static constexpr const uint8_t kCompressionNormal = 2;

  constexpr uint8_t compression_type() const {
    // Unclear what the actual bitmask is.
    return (type_ & 0x1f);
  }
} __attribute__((packed));

static_assert(sizeof(ArchivedPackMode) == 1);


class ArchivedFileHeader {
 private:
  struct {
    uint8_t           attributes_;       // File protection modes
    uint8_t           reserved1_;        // Reserved
    uint32_t          unpack_size_;      // Unpacked size
    uint32_t          pack_size_;        // Packed size
    uint8_t           machine_type_;     // Machine type
    ArchivedPackMode  pack_mode_;        // Pack mode
    uint8_t           flags_;            // Flags
    uint8_t           reserved2_;        // Reserved
    uint8_t           comment_length_;   // Comment length
    uint8_t           extract_ver_;      // Version needed to extract
    uint8_t           reserved3_[2];     // Reserved
    ArchivedDateStamp date_;             // Packed date
    uint32_t          data_crc_;         // Data CRC
    uint32_t          header_crc_;       // Header CRC
    uint8_t           filename_length_;  // Filename length
  } __attribute__((packed)) metadata_;

  static_assert(sizeof(metadata_) == 31);

  std::string filename_;
  std::string comment_;

 public:
  void clear_header_crc() {
    metadata_.header_crc_ = 0;
  }

  constexpr uint32_t unpack_size() const {
    return data_to_host(metadata_.unpack_size_);
  }

  constexpr uint32_t pack_size() const {
    return data_to_host(metadata_.pack_size_);
  }

  constexpr const ArchivedDateStamp& datestamp() const {
    return metadata_.date_;
  }

  constexpr uint32_t data_crc() const {
    return data_to_host(metadata_.data_crc_);
  }

  constexpr uint32_t header_crc() const {
    return data_to_host(metadata_.header_crc_);
  }

  constexpr const ArchivedPackMode& pack_mode() const {
    return metadata_.pack_mode_;
  }

  constexpr uint8_t filename_length() const {
    return metadata_.filename_length_;
  }

  constexpr uint8_t comment_length() const {
    return metadata_.comment_length_;
  }

  constexpr uint8_t attributes() const {
    return metadata_.attributes_;
  }

  constexpr uint8_t flags() const {
    return metadata_.flags_;
  }

  constexpr const std::string& filename() const {
    return filename_;
  }

  constexpr const std::string& comment() const {
    return comment_;
  }

  static std::unique_ptr<ArchivedFileHeader> from_buffer(InputBuffer* buffer);
};

namespace crc {
void     reset();
uint32_t calc(const void* memory, size_t length);
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
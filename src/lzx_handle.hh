#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "mmap_buffer.hh"

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

union ProtectionBits {
  uint8_t raw;
  struct {
    uint8_t h : 1;
    uint8_t s : 1;
    uint8_t p : 1;
    uint8_t a : 1;
    uint8_t r : 1;
    uint8_t w : 1;
    uint8_t e : 1;
    uint8_t d : 1;
  };
} __attribute__((packed));

static_assert(sizeof(ProtectionBits) == 1);

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
    ProtectionBits    attributes_;       // File protection modes
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
    return metadata_.attributes_.raw;
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

  bool is_merged() const {
    return (flags() & 1) != 0;
  }

  std::string attributes_str() const;

  static std::unique_ptr<ArchivedFileHeader> from_buffer(InputBuffer* buffer);
};

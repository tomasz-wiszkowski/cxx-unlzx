#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>

#include "error.hh"
#include "mmap_buffer.hh"
#include "types.hh"

namespace lzx {
/// @brief A bitfield representing the protection bits of a file.
union ProtectionBits {
  uint8_t raw;
  struct {
    uint8_t readable : 1;
    uint8_t writable : 1;
    uint8_t deletable : 1;
    uint8_t executable : 1;
    uint8_t archived : 1;
    uint8_t hidden : 1;
    uint8_t script : 1;
    uint8_t pure : 1;
  };

  ProtectionBits() : raw(0) {}
  explicit ProtectionBits(uint8_t from) : raw(from) {}
} PACKED;
static_assert(sizeof(ProtectionBits) == 1);


/// @brief A datestamp representing the date and time stamp of a file.
class DateStamp {
  union DateStampInternal {
    typedef uint32_t RealType;

    RealType raw;

    struct {
      uint32_t seconds : 6;
      uint32_t minutes : 6;
      uint32_t hours : 5;
      uint32_t year : 6;
      uint32_t month : 4;
      uint32_t day : 5;
    };

    DateStampInternal(RealType from) : raw(from) {}
  };

  TypedValue<DateStampInternal, std::endian::big> stamp_;

 public:
  /**
   * @brief Gets the year from the datestamp.
   * @return The year (since 1970).
   */
  constexpr uint32_t year() const {
    return stamp_.value().year + 1970;
  }

  /**
   * @brief Gets the month from the datestamp.
   * @return The month (1-12).
   */
  constexpr uint32_t month() const {
    return stamp_.value().month;
  }

  /**
   * @brief Gets the day from the datestamp.
   * @return The day of the month (1-31).
   */
  constexpr uint32_t day() const {
    return stamp_.value().day;
  }

  /**
   * @brief Gets the hour from the datestamp.
   * @return The hour (0-23).
   */
  constexpr uint32_t hour() const {
    return stamp_.value().hours;
  }

  /**
   * @brief Gets the minute from the datestamp.
   * @return The minute (0-59).
   */
  constexpr uint32_t minute() const {
    return stamp_.value().minutes;
  }

  /**
   * @brief Gets the second from the datestamp.
   * @return The second (0-59).
   */
  constexpr uint32_t second() const {
    return stamp_.value().seconds;
  }
} PACKED;
static_assert(sizeof(DateStamp) == 4);


class CompressionInfo {
 public:
  enum class Mode : uint8_t {
    kNone   = 0,
    kNormal = 2,
  };

  /**
   * @brief Gets the compression mode.
   * @return The compression Mode enum value.
   */
  constexpr Mode mode() const {
    return mode_;
  }

 private:
  Mode    mode_ : 5;  // Guessed.
  uint8_t       : 3;  // Reserved.

} PACKED;
static_assert(sizeof(CompressionInfo) == 1);


class Flags {
 public:
  /**
   * @brief Checks if the entry is merged.
   * @return True if the entry is merged, false otherwise.
   */
  constexpr bool is_merged() const {
    return merged_;
  }

 private:
  uint8_t merged_ : 1;
  uint8_t         : 7;  // Reserved.

} PACKED;
static_assert(sizeof(CompressionInfo) == 1);


class Entry {
 private:
  struct {
    ProtectionBits                       attributes_;        // File protection modes
    uint8_t                              reserved1_;         // Reserved
    Value<uint32_t, std::endian::little> unpack_size_;       // Unpacked size
    Value<uint32_t, std::endian::little> pack_size_;         // Packed size
    uint8_t                              machine_type_;      // Machine type
    CompressionInfo                      compression_info_;  // Compression info
    Flags                                flags_;             // Flags
    uint8_t                              reserved2_;         // Reserved
    uint8_t                              comment_length_;    // Comment length
    uint8_t                              extract_ver_;       // Version needed to extract
    uint8_t                              reserved3_[2];      // Reserved
    DateStamp                            date_;              // Packed date
    Value<uint32_t, std::endian::little> data_crc_;          // Data CRC
    Value<uint32_t, std::endian::little> header_crc_;        // Header CRC
    uint8_t                              filename_length_;   // Filename length
  } PACKED metadata_;

  static_assert(sizeof(metadata_) == 31);

  std::string filename_;
  std::string comment_;

 public:
  /**
   * @brief Gets the unpack size of the entry.
   * @return The unpacked size in bytes.
   */
  constexpr size_t unpack_size() const {
    return metadata_.unpack_size_.value();
  }

  /**
   * @brief Gets the packed size of the entry.
   * @return The packed size in bytes.
   */
  constexpr size_t pack_size() const {
    return metadata_.pack_size_.value();
  }

  /**
   * @brief Gets the datestamp of the entry.
   * @return The DateStamp object.
   */
  constexpr const DateStamp& datestamp() const {
    return metadata_.date_;
  }

  /**
   * @brief Gets the data CRC-32 of the entry.
   * @return The CRC-32 value.
   */
  constexpr uint32_t data_crc() const {
    return metadata_.data_crc_.value();
  }

  /**
   * @brief Gets the compression info of the entry.
   * @return The CompressionInfo object.
   */
  constexpr CompressionInfo compression_info() const {
    return metadata_.compression_info_;
  }

  /**
   * @brief Gets the protection bits (attributes) of the entry.
   * @return The ProtectionBits object.
   */
  constexpr ProtectionBits attributes() const {
    return metadata_.attributes_;
  }

  /**
   * @brief Gets the flags of the entry.
   * @return The Flags object.
   */
  constexpr Flags flags() const {
    return metadata_.flags_;
  }

  /**
   * @brief Gets the filename of the entry.
   * @return The filename string.
   */
  constexpr const std::string& filename() const {
    return filename_;
  }

  /**
   * @brief Gets the comment of the entry.
   * @return The comment string.
   */
  constexpr const std::string& comment() const {
    return comment_;
  }

  /**
   * @brief Reads an Entry from the given buffer.
   * @param buffer The input buffer to read from.
   * @param out A unique pointer to store the parsed Entry.
   * @return Status indicating success or the specific error encountered.
   */
  static Status from_buffer(InputBuffer* buffer, std::unique_ptr<Entry>& out);
};

}  // namespace lzx


/// @brief A custom formatter for ProtectionBits.
template <>
struct std::formatter<lzx::ProtectionBits> : std::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(lzx::ProtectionBits bits, FormatContext& ctx) const {
    std::string result = "--------";
    if (bits.hidden) result[0] = 'h';
    if (bits.script) result[1] = 's';
    if (bits.pure) result[2] = 'p';
    if (bits.archived) result[3] = 'a';
    if (bits.readable) result[4] = 'r';
    if (bits.writable) result[5] = 'w';
    if (bits.executable) result[6] = 'e';
    if (bits.deletable) result[7] = 'd';

    return formatter<std::string_view>::format(std::move(result), ctx);
  }
};

/// @brief A custom formatter for DateStamp.
template <>
struct std::formatter<lzx::DateStamp> : std::formatter<std::string_view> {
  enum class Format : uint8_t {
    kDate,
    kTime,
  };

  Format format_ = Format::kDate;

  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext& ctx) {
    auto it = ctx.begin();

    while (it != ctx.end() && *it != '}') {
      char format = *it++;
      switch (format) {
      case 'd':  // {:d} -- display date.
        format_ = Format::kDate;
        break;

      case 't':  // {:t} -- display time.
        format_ = Format::kTime;
        break;
      }
    }

    return it;
  }

  template <typename FormatContext>
  auto format(const lzx::DateStamp& stamp, FormatContext& ctx) const {
    switch (format_) {
    case Format::kTime:
      formatter<std::string_view>::format(
          std::format("{:02}:{:02}:{:02}", stamp.hour(), stamp.minute(), stamp.second()), ctx);
      break;

    case Format::kDate:
      formatter<std::string_view>::format(
          std::format("{:02}-{:02}-{:4}", stamp.day(), stamp.month(), stamp.year()), ctx);
      break;
    }

    return ctx.out();
  }
};

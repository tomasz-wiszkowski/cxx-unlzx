#include "crc.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace crc {
namespace {

// Use `std::bit_reverse()` when available.
template <typename T>
constexpr T bit_reverse(T value) {
  T result = 0;
  for (size_t bit = 0; bit < sizeof(value) * 8; ++bit) {
    result <<= 1;
    result |= value & 1;
    value >>= 1;
  }
  return result;
}

/// The CRC32 polynomial without the x^32 bit set.
constexpr uint32_t kCrc32Polynomial = 0x04C11DB7;
/// The reversed CRC32 polynomial without the x^0 bit set, used to compute the lookup table.
constexpr uint32_t kCrc32ReversedPolynomial = bit_reverse(kCrc32Polynomial);
static_assert(kCrc32ReversedPolynomial == 0xEDB88320);

/// Generate a CRC32 polynomial table.
///
/// This function generates a table of 256 CRC32 values based on the reversed CRC32 polynomial.
/// The table is used to efficiently compute the CRC32 checksum of data.
///
/// @return A constexpr std::array containing 256 CRC32 values.
///
constexpr std::array<uint32_t, 256> generate_lookup_table() {
  std::array<uint32_t, 256> table;

  std::generate(table.begin(), table.end(), [index = 0]() mutable {
    uint32_t crc = index++;
    for (uint32_t j = 0; j < 8; ++j) {
      bool lsb = (crc & 1) != 0;
      crc >>= 1;
      if (lsb) {
        crc ^= kCrc32ReversedPolynomial;
      }
    }
    return crc;
  });

  return table;
}

/// The CRC32 lookup table.
constexpr std::array<uint32_t, 256> crc_table = generate_lookup_table();

}  // namespace

auto Crc32::sum() const -> uint32_t {
  return sum_;
}

auto Crc32::calc(const void* memory_raw, size_t length) -> uint32_t {
  const uint8_t* memory = static_cast<const uint8_t*>(memory_raw);

  if (length == 0) {
    return sum_;
  }

  uint32_t temp = ~sum_;
  for (size_t i = 0; i < length; ++i) {
    temp = crc_table[(memory[i] ^ temp) & 255] ^ (temp >> 8);
  }
  sum_ = ~temp;
  return sum_;
}
}  // namespace crc

#pragma once

#include <cstddef>
#include <cstdint>

namespace crc {
class Crc32 {
 public:
  /**
   * @brief Constructs a new Crc32 instance, initializing the sum to 0.
   */
  Crc32() : sum_(0) {}

  /**
   * @brief Calculates the CRC-32 for the given memory buffer.
   * @param memory Pointer to the data.
   * @param length Length of the data in bytes.
   * @return The calculated CRC-32 value.
   */
  uint32_t calc(const void* memory, size_t length);

  /**
   * @brief Gets the current CRC-32 sum.
   * @return The accumulated CRC-32 value.
   */
  uint32_t sum() const;

 private:
  uint32_t sum_ = 0;
};
}  // namespace crc

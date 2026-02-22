#pragma once

#include <cstddef>
#include <cstdint>

namespace crc {
class Crc32 {
 public:
  Crc32() : sum_(0) {}
  uint32_t calc(const void* memory, size_t length);
  uint32_t sum() const;

 private:
  uint32_t sum_ = 0;
};
}  // namespace crc

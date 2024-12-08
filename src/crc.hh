#pragma once

#include <cstddef>
#include <cstdint>

namespace crc {
void     reset();
uint32_t calc(const void* memory, size_t length);
uint32_t sum();
}  // namespace crc

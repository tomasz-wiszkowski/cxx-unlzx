#pragma once

#include <stdint.h>

#include "circular_buffer.hh"
#include "mmap_buffer.hh"

enum class Action : uint8_t { View, Extract };

void process_archive(char* filename, Action action);

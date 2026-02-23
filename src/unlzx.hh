#pragma once

#include <stdint.h>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "error.hh"
#include "mmap_buffer.hh"
#include "lzx_handle.hh"
#include "lzx_block.hh"
#include "lzx_entry.hh"

class Unlzx {
public:
  Status open_archive(const char* filename);
  std::map<std::string, LzxEntry> list_archive();

private:
  std::unique_ptr<MmapInputBuffer> mmap_buffer_;
  std::optional<InputBuffer> in_buffer_;
};

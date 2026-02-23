#pragma once

#include <stdint.h>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "circular_buffer.hh"
#include "error.hh"
#include "mmap_buffer.hh"
#include "lzx_handle.hh"
#include "lzx_block.hh"
#include "lzx_entry.hh"

enum class Action : uint8_t { View, Extract };

class Unlzx {
public:
  Status open_archive(const char* filename);
  Status extract_archive();
  std::map<std::string, LzxEntry> list_archive();

private:
  Status extract_normal(InputBuffer in_file);
  Status extract_store(InputBuffer in_file);
  void report_unknown();
  Status open_output(lzx::Entry* node, std::unique_ptr<FILE, decltype(&std::fclose)>& out);

  std::deque<std::unique_ptr<lzx::Entry>> merged_files_;
  std::unique_ptr<MmapInputBuffer> mmap_buffer_;
  std::optional<InputBuffer> in_buffer_;
};

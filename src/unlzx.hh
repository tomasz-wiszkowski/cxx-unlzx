#pragma once

#include <stdint.h>
#include <deque>
#include <memory>
#include <optional>
#include <string>

#include "circular_buffer.hh"
#include "error.hh"
#include "mmap_buffer.hh"
#include "lzx_handle.hh"

enum class Action : uint8_t { View, Extract };

class Unlzx {
public:
  Status process_archive(const char* filename, Action action);

private:
  Status extract_archive();
  Status list_archive();
  Status extract_normal(InputBuffer in_file);
  Status extract_store(InputBuffer in_file);
  void report_unknown();
  Status open_output(lzx::Entry* node, std::unique_ptr<FILE, decltype(&std::fclose)>& out);

  std::deque<std::unique_ptr<lzx::Entry>> merged_files_;
  std::unique_ptr<MmapInputBuffer> mmap_buffer_;
  std::optional<InputBuffer> in_buffer_;
};

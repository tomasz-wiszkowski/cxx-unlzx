#pragma once

#include <stdint.h>
#include <deque>
#include <memory>
#include <optional>
#include <string>

#include "circular_buffer.hh"
#include "mmap_buffer.hh"
#include "lzx_handle.hh"

enum class Action : uint8_t { View, Extract };

class Unlzx {
public:
  void process_archive(const char* filename, Action action);

private:
  void extract_archive();
  void list_archive();
  void extract_normal(InputBuffer in_file);
  void extract_store(InputBuffer in_file);
  void report_unknown();
  std::unique_ptr<FILE, decltype(&std::fclose)> open_output(lzx::Entry* node);

  std::deque<std::unique_ptr<lzx::Entry>> merged_files_;
  std::unique_ptr<MmapInputBuffer> mmap_buffer_;
  std::optional<InputBuffer> in_buffer_;
};

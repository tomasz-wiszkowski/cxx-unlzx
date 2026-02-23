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
  /**
   * @brief Opens an LZX archive from the given filename.
   * @param filename The path to the LZX archive file.
   * @return Status indicating success or the specific error encountered.
   */
  Status open_archive(const char* filename);

  /**
   * @brief Lists the contents of the opened archive.
   * @return A map of filenames to their corresponding LzxEntry objects.
   */
  std::map<std::string, LzxEntry> list_archive();

private:
  std::unique_ptr<MmapInputBuffer> mmap_buffer_;
  std::optional<InputBuffer> in_buffer_;
};

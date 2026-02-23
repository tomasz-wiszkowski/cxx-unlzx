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

enum class Action : uint8_t { View, Extract };

struct LzxFileSegment {
  std::shared_ptr<LzxBlock> block;
  size_t decompressed_offset;
  size_t decompressed_length;
};

struct LzxEntry {
  std::string name;
  lzx::Entry metadata;
  std::vector<LzxFileSegment> segments;

  std::optional<size_t> pack_size() const {
    if (metadata.flags().is_merged()) {
      size_t total_guessed = 0;
      for (const auto& segment : segments) {
        if (segment.block) {
          size_t block_unpacked = segment.block->total_unpacked_size();
          if (block_unpacked > 0) {
            double ratio = static_cast<double>(segment.decompressed_length) / block_unpacked;
            total_guessed += static_cast<size_t>(ratio * segment.block->packed_size());
          }
        }
      }
      return total_guessed;
    }
    return metadata.pack_size();
  }

  size_t unpack_size() const {
    size_t total = 0;
    for (const auto& segment : segments) {
      total += segment.decompressed_length;
    }
    return total;
  }

  lzx::DateStamp datestamp() const {
    return metadata.datestamp();
  }

  lzx::ProtectionBits attributes() const {
    return metadata.attributes();
  }

  const std::string& comment() const {
    return metadata.comment();
  }
};

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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lzx_block.hh"
#include "lzx_entry.hh"
#include "lzx_handle.hh"

class LzxEntryBuilder {
public:
  /**
   * @brief Constructs an LzxEntryBuilder with the given metadata.
   * @param metadata The lzx::Entry metadata for the entry being built.
   */
  explicit LzxEntryBuilder(lzx::Entry metadata);

  /**
   * @brief Adds a file segment to the entry being built.
   * @param block The block associated with this segment.
   * @param decompressed_offset The decompressed offset.
   * @param decompressed_length The decompressed length.
   */
  void add_segment(std::shared_ptr<LzxBlock> block, size_t decompressed_offset, size_t decompressed_length);

  /**
   * @brief Builds and returns the final LzxEntry.
   * @param name The name of the entry.
   * @return The constructed LzxEntry.
   */
  LzxEntry build(std::string name);

private:
  lzx::Entry metadata_;
  std::vector<LzxFileSegment> segments_;
};

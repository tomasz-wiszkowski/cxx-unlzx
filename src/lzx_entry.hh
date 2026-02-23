#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "lzx_block.hh"
#include "lzx_handle.hh"

class LzxFileSegment {
public:
  /**
   * @brief Constructs an LzxFileSegment with specific values.
   * @param block The block associated with this segment.
   * @param decompressed_offset The decompressed offset.
   * @param decompressed_length The decompressed length.
   */
  LzxFileSegment(std::shared_ptr<LzxBlock> block, size_t decompressed_offset, size_t decompressed_length);

  /**
   * @brief Gets the associated LzxBlock.
   * @return The shared pointer to the LzxBlock.
   */
  std::shared_ptr<LzxBlock> block() const;

  /**
   * @brief Gets the segment of the block data.
   * @return The span to the segment data.
   */
  std::span<const uint8_t> get_data() const;

  /**
   * @brief Gets the status of the last decompression operation from the block.
   * @return The Status code.
   */
  Status get_status() const;

  /**
   * @brief Gets the decompressed length.
   * @return The decompressed length.
   */
  size_t decompressed_length() const;

private:
  std::shared_ptr<LzxBlock> block_;
  size_t decompressed_offset_{0};
  size_t decompressed_length_{0};
};

class LzxEntry {
public:
  /**
   * @brief Constructs an LzxEntry with metadata and segments.
   * @param name The name of the entry.
   * @param metadata The lzx::Entry metadata.
   * @param segments The file segments associated with this entry.
   */
  LzxEntry(std::string name, lzx::Entry metadata, std::vector<LzxFileSegment> segments);

  /**
   * @brief Gets the name of the entry.
   * @return The name string.
   */
  const std::string& name() const;

  /**
   * @brief Gets the metadata for the entry.
   * @return The lzx::Entry metadata.
   */
  const lzx::Entry& metadata() const;

  /**
   * @brief Gets the file segments for the entry.
   * @return The vector of LzxFileSegment.
   */
  const std::vector<LzxFileSegment>& segments() const;

  /**
   * @brief Calculates the estimated pack size for the entry.
   * @return The pack size if it can be determined, std::nullopt otherwise.
   */
  std::optional<size_t> pack_size() const;

  /**
   * @brief Calculates the total unpack size for the entry.
   * @return The total decompressed length of all segments.
   */
  size_t unpack_size() const;

  /**
   * @brief Gets the datestamp of the entry.
   * @return The datestamp.
   */
  lzx::DateStamp datestamp() const;

  /**
   * @brief Gets the protection bits (attributes) of the entry.
   * @return The protection bits.
   */
  lzx::ProtectionBits attributes() const;

  /**
   * @brief Gets the comment associated with the entry.
   * @return The comment string.
   */
  const std::string& comment() const;

private:
  std::string name_;
  lzx::Entry metadata_;
  std::vector<LzxFileSegment> segments_;
};

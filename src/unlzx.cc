/* $VER: unlzx.c 1.0 (22.2.98) */
/* Created: 11.2.98 */
/* Added Pipe support to read from stdin (03.4.01, Erik Meusel)           */

/* LZX Extract in (supposedly) portable C.                                */

/* Compile with:                                                          */
/* gcc unlzx.c -ounlzx -O6                                                */

/* Thanks to Dan Fraser for decoding the coredumps and helping me track   */
/* down some HIDEOUSLY ANNOYING bugs.                                     */

/* Everything is accessed as unsigned char's to try and avoid problems    */
/* with byte order and alignment. Most of the decrunch functions          */
/* encourage overruns in the buffers to make things as fast as possible.  */
/* All the time is taken up in crc::calc() and decrunch() so they are      */
/* pretty damn optimized. Don't try to understand this program.           */

/* ---------------------------------------------------------------------- */
#include "unlzx.hh"

#include <sys/types.h>

#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>

#include "crc.hh"
#include "huffman_decoder.hh"
#include "lzx_entry_builder.hh"
#include "lzx_handle.hh"

/* ---------------------------------------------------------------------- */

std::map<std::string, LzxEntry> Unlzx::list_archive() {
  std::map<std::string, LzxEntryBuilder> builders;

  std::unique_ptr<lzx::Entry> archive_header;
  size_t current_decompressed_offset = 0;
  struct PendingMerge {
    std::string filename;
    size_t      offset;
    size_t      length;
  };
  std::vector<PendingMerge> pending_merges;

  if (!in_buffer_) return {};

  while (!in_buffer_->is_eof()) {
    if (lzx::Entry::from_buffer(&*in_buffer_, archive_header) != Status::Ok) {
      break;
    }

    size_t      pack_size   = archive_header->pack_size();
    std::string filename    = archive_header->filename();
    bool        is_merged   = archive_header->flags().is_merged();
    uint32_t    unpack_size = archive_header->unpack_size();

    auto [it, inserted] = builders.try_emplace(filename, *archive_header);

    if (is_merged) {
      pending_merges.push_back({filename, current_decompressed_offset, unpack_size});
      current_decompressed_offset += unpack_size;

      if (pack_size > 0) {
        InputBuffer sub = *in_buffer_;
        InputBuffer block_data;
        sub.read_buffer(pack_size, block_data);

        auto shared_block = std::make_shared<LzxBlock>(std::move(*archive_header), block_data, current_decompressed_offset);
        for (const auto& pending : pending_merges) {
          builders.at(pending.filename).add_segment(shared_block, pending.offset, pending.length);
        }
        pending_merges.clear();
        current_decompressed_offset = 0;
      }
    } else {
      InputBuffer sub = *in_buffer_;
      InputBuffer block_data;
      sub.read_buffer(pack_size, block_data);

      auto shared_block = std::make_shared<LzxBlock>(std::move(*archive_header), block_data, unpack_size);
      builders.at(filename).add_segment(shared_block, 0, unpack_size);
    }

    if (in_buffer_->skip(pack_size) != Status::Ok) {
      break;
    }
  }

  std::map<std::string, LzxEntry> entries;
  for (auto& [name, builder] : builders) {
    entries.try_emplace(name, builder.build(name));
  }

  return entries;
}

Status Unlzx::open_archive(const char* filename) {
  TRY(MmapInputBuffer::for_file(filename, mmap_buffer_));

  in_buffer_ = mmap_buffer_->get();

  uint8_t header[10]{};

  TRY(in_buffer_->read_into(header, sizeof(header)));

  if ((header[0] != 'L') || (header[1] != 'Z') || (header[2] != 'X')) {
    return Status::NotLzxFile;
  }

  return Status::Ok;
}

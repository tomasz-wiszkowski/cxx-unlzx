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

#include "circular_buffer.hh"
#include "crc.hh"
#include "huffman_decoder.hh"
#include "lzx_handle.hh"

/* ---------------------------------------------------------------------- */

/* Opens a file for writing & creates the full path if required. */
auto Unlzx::open_output(lzx::Entry* node) -> std::unique_ptr<FILE, decltype(&std::fclose)> {
  std::unique_ptr<FILE, decltype(&std::fclose)> file(nullptr, &std::fclose);

  // Special case: create directory.
  if (node->unpack_size() == 0 && node->filename().ends_with("/")) {
    std::filesystem::create_directories(node->filename());
    std::print("Creating directory \"{}\"", node->filename());
    return file;
  }

  file.reset(fopen(node->filename().data(), "wbe"));

  if (!file) {
    // Compute the name of the encompassing directory and create it.
    // This logic assumes the file could not be opened because the parent directory doesn't exist.
    std::string dirname(node->filename());
    auto        pos = dirname.rfind('/');
    if (pos != std::string::npos) {
      dirname.resize(pos);
      std::println("Creating \"{}/\"", dirname);
      std::filesystem::create_directories(dirname);
    }

    file.reset(fopen(node->filename().data(), "wbe"));
  }

  if (!file) {
    throw std::runtime_error(std::format("unable to create file \"{}\"", node->filename()));
  }

  std::print("Writing \"{}\"...", node->filename());
  return file;
}

/* ---------------------------------------------------------------------- */

auto Unlzx::extract_normal(InputBuffer in_file) -> void {
  huffman::HuffmanDecoder decoder;
  uint32_t                unpack_size     = 0;
  size_t                  decrunch_length = 0;

  CircularBuffer<uint8_t> buffer(65536);

  while (!merged_files_.empty()) {
    auto node = std::move(merged_files_.front());
    merged_files_.pop_front();
    auto out_file = open_output(node.get());

    crc::Crc32 crc_calc;

    unpack_size = node->unpack_size();
    while (unpack_size > 0) {
      if (buffer.is_empty()) {
        if (decrunch_length <= 0) {
          decoder.read_literal_table(&in_file);
          decrunch_length = decoder.decrunch_length();
        }

        decoder.decrunch(&in_file, &buffer, std::min(decrunch_length, size_t(65536 - 256)));
        size_t have_bytes = std::min(decrunch_length, buffer.size());
        decrunch_length -= have_bytes;
      }

      auto spans = buffer.read(unpack_size);
      for (auto& span : spans) {
        /* calculate amount of data we can use before we need to fill the buffer again */
        size_t count = std::min(span.size(), size_t(unpack_size));

        crc_calc.calc(span.data(), count);

        if (out_file) {
          if (fwrite(span.data(), 1, count, out_file.get()) != count) {
            throw std::runtime_error(std::format("could not write file \"{}\"", node->filename()));
          }
        }

        unpack_size -= count;
      }
    }

    std::println(" crc {}", (node->data_crc() == crc_calc.sum()) ? "good" : "bad");
  }
}

/* ---------------------------------------------------------------------- */

auto Unlzx::extract_store(InputBuffer in_file) -> void {
  while (!merged_files_.empty()) {
    auto node = std::move(merged_files_.front());
    merged_files_.pop_front();

    uint32_t unpack_size = std::min(in_file.available(), node->unpack_size());

    auto view     = in_file.read_span(unpack_size);
    auto out_file = open_output(node.get());
    auto written  = 0;

    if (out_file) {
      written = fwrite(view.data(), 1, view.size(), out_file.get());
    }

    if (written != view.size()) {
      throw std::runtime_error(std::format("could not write file \"{}\"", node->filename()));
    }

    crc::Crc32 crc_calc;
    crc_calc.calc(view.data(), view.size());
    std::println(" crc {}", (node->data_crc() == crc_calc.sum()) ? "good" : "bad");
  }
}

/* ---------------------------------------------------------------------- */

auto Unlzx::report_unknown() -> void {
  while (!merged_files_.empty()) {
    auto node = std::move(merged_files_.front());
    merged_files_.pop_front();
    std::println("Skipping \"{}\": unknown compression mode.", node->filename());
  }
}

/* ---------------------------------------------------------------------- */

auto Unlzx::extract_archive() -> void {
  while (auto archive_header = lzx::Entry::from_buffer(&*in_buffer_)) {
    size_t pack_size        = archive_header->pack_size();
    auto   compression_type = archive_header->compression_info().mode();

    merged_files_.emplace_back(std::move(archive_header));

    // Unpack merged files.
    if (pack_size != 0U) {
      switch (compression_type) {
      case lzx::CompressionInfo::Mode::kNone:
        extract_store(in_buffer_->read_buffer(pack_size));
        break;

      case lzx::CompressionInfo::Mode::kNormal:
        extract_normal(in_buffer_->read_buffer(pack_size));
        break;

      default:
        report_unknown();
        in_buffer_->skip(pack_size);
        break;
      }
    }
  }
}

/* ---------------------------------------------------------------------- */

auto Unlzx::list_archive() -> void {
  size_t total_pack   = 0;
  size_t total_unpack = 0;
  size_t total_files  = 0;
  size_t merge_size   = 0;

  std::println("Unpacked Packed   Time     Date       Attrib   Name");
  std::println("-------- -------- -------- ---------- -------- ----");

  while (auto archive_header = lzx::Entry::from_buffer(&*in_buffer_)) {
    uint32_t    unpack_size = archive_header->unpack_size();
    size_t      pack_size   = archive_header->pack_size();
    const auto& stamp       = archive_header->datestamp();
    const auto& attrs       = archive_header->attributes();

    total_pack += pack_size;
    total_unpack += unpack_size;
    total_files++;
    merge_size += unpack_size;

    std::print("{:8} ", unpack_size);
    if (archive_header->flags().is_merged()) {
      std::print("     n/a ");
    } else {
      std::print("{:8} ", pack_size);
    }

    std::print("{0:t} {0:d} {1} ", stamp, attrs);

    std::println("\"{}\"", archive_header->filename());
    if (!archive_header->comment().empty()) {
      std::println(": \"{}\"", archive_header->comment());
    }

    if (archive_header->flags().is_merged() && (pack_size > 0)) {
      std::println("{:8} {:8} Merged", merge_size, pack_size);
      merge_size = 0;
    }

    in_buffer_->skip(pack_size);
  }

  std::println("-------- -------- -------- ---------- -------- ----");
  std::print("{:8} {:8} ", total_unpack, total_pack);
  std::println("{} file{}", total_files, ((total_files == 1) ? "" : "s"));
}

auto Unlzx::process_archive(const char* filename, Action action) -> void {
  mmap_buffer_ = MmapInputBuffer::for_file(filename);
  if (!mmap_buffer_) {
    throw std::runtime_error("could not open file");
  }
  in_buffer_ = mmap_buffer_->get();

  uint8_t header[10]{};

  in_buffer_->read_into(header, sizeof(header));

  if ((header[0] != 'L') || (header[1] != 'Z') || (header[2] != 'X')) {
    throw std::runtime_error("not an LZX file");
  }

  switch (action) {
  case Action::Extract:
    extract_archive();
    break;

  case Action::View:
    list_archive();
    break;
  }
}

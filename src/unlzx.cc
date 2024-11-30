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
#include "unlzx.h"

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

/* ---------------------------------------------------------------------- */

static const unsigned char kVersion[] = "$VER: unlzx 1.1 (03.4.01)";

/* ---------------------------------------------------------------------- */

static unsigned char info_header[10];

static unsigned int pack_size;

static std::deque<std::unique_ptr<ArchivedFileHeader>> merged_files;

/* ---------------------------------------------------------------------- */

uint8_t  decrunch_buffer[258 + 65536 + 258]; /* allow overrun for speed */
uint8_t* destination;
uint8_t* destination_end;

/* ---------------------------------------------------------------------- */

static const char* month_str[16] = {"?00", "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug",
    "sep", "oct", "nov", "dec", "?13", "?14", "?15"};

/* Opens a file for writing & creates the full path if required. */
static auto open_output(const std::string& filename)
    -> std::unique_ptr<FILE, decltype(&std::fclose)> {
  std::unique_ptr<FILE, decltype(&std::fclose)> file(nullptr, &std::fclose);

  file.reset(fopen(filename.data(), "wbe"));

  if (!file) {
    // Compute the name of the encompassing directory and create it.
    // This logic assumes the file could not be opened because the parent directory doesn't exist.
    std::string dirname(filename);
    auto        pos = dirname.rfind('/');
    if (pos != std::string::npos) {
      dirname.resize(pos);
      std::println("Creating \"{}/\"", dirname);
      std::filesystem::create_directories(dirname);
    }

    file.reset(fopen(filename.data(), "wbe"));
  }

  if (!file) {
    throw std::runtime_error(std::format("unable to create file \"{}\"", filename));
  }

  std::print("Writing \"{}\"...", filename);
  return file;
}

/* ---------------------------------------------------------------------- */

auto ArchivedFileHeader::from_buffer(InputBuffer* buffer) -> std::unique_ptr<ArchivedFileHeader> {
  if (buffer->is_eof()) return {};

  auto result = std::make_unique<ArchivedFileHeader>();
  buffer->read_into(&result->metadata_, sizeof(result->metadata_));

  uint32_t const header_crc = result->header_crc();
  result->clear_header_crc();
  result->filename_ = buffer->read_string_view(result->filename_length());
  result->comment_  = buffer->read_string_view(result->comment_length());

  crc::reset();
  crc::calc(&result->metadata_, sizeof(result->metadata_));
  crc::calc(result->filename_.data(), result->filename_.size());
  crc::calc(result->comment_.data(), result->comment_.size());
  if (crc::sum() != header_crc) {
    throw std::runtime_error("File header checksum invalid.");
  }

  return result;
}

static auto extract_normal(InputBuffer* in_file) -> void {
  unsigned char* pos   = nullptr;
  unsigned char* temp  = nullptr;
  unsigned int   count = 0;

  huffman::HuffmanDecoder decoder;

  uint32_t unpack_size     = 0;
  int64_t  decrunch_length = 0;

  auto compressed_data = in_file->read_buffer(pack_size);
  pack_size            = 0;

  pos = destination_end = destination = decrunch_buffer + 258 + 65536;

  while (!merged_files.empty()) {
    auto node = std::move(merged_files.front());
    merged_files.pop_front();
    auto out_file = open_output(node->filename());

    crc::reset();

    unpack_size = node->unpack_size();
    while (unpack_size > 0) {
      if (pos == destination) {
        if (decrunch_length <= 0) {
          if (decoder.read_literal_table(&compressed_data) != 0) {
            break; /* argh! can't make huffman tables! */
          }
          decrunch_length = decoder.decrunch_length();
        }

        /* unpack some data */
        if (destination >= decrunch_buffer + 258 + 65536) {
          count = destination - decrunch_buffer - 65536;
          if (count != 0U) {
            temp = (destination = decrunch_buffer) + 65536;
            do { /* copy the overrun to the start of the buffer */
              *destination++ = *temp++;
            } while (--count != 0U);
          }
          pos = destination;
        }
        destination_end = destination + decrunch_length;
        destination_end = std::min(destination_end, decrunch_buffer + 258 + 65536);
        temp            = destination;

        decoder.decrunch(&compressed_data);

        decrunch_length -= (destination - temp);
      }

      /* calculate amount of data we can use before we need to fill the buffer again */
      count = destination - pos;
      count = std::min(count, unpack_size); /* take only what we need */

      crc::calc(pos, count);

      if (fwrite(pos, 1, count, out_file.get()) != count) {
        throw std::runtime_error(std::format("could not write file \"{}\"", node->filename()));
      }
      unpack_size -= count;
      pos += count;
    }

    std::println(" crc {}", (node->data_crc() == crc::sum()) ? "good" : "bad");
  } /* for */
}

/* ---------------------------------------------------------------------- */

static auto extract_store(InputBuffer* in_file) -> void {
  while (!merged_files.empty()) {
    auto node = std::move(merged_files.front());
    merged_files.pop_front();

    uint32_t unpack_size = std::min(pack_size, node->unpack_size());

    auto view     = in_file->read_span(unpack_size);
    auto out_file = open_output(node->filename());
    auto written  = fwrite(view.data(), 1, view.size(), out_file.get());

    if (written != view.size()) {
      throw std::runtime_error(std::format("could not write file \"{}\"", node->filename()));
    }

    crc::reset();
    crc::calc(view.data(), view.size());
    std::println(" crc {}", (node->data_crc() == crc::sum()) ? "good" : "bad");

    pack_size -= view.size();
  }
}

/* ---------------------------------------------------------------------- */

static auto report_unknown() -> void {
  while (!merged_files.empty()) {
    auto node = std::move(merged_files.front());
    merged_files.pop_front();
    std::println("Skipping \"{}\": unknown compression mode.", node->filename());
  }
}

/* ---------------------------------------------------------------------- */

static auto extract_archive(InputBuffer* in_file) -> void {
  for (;;) {
    auto archive_header = ArchivedFileHeader::from_buffer(in_file);
    if (!archive_header) {
      break;
    }

    pack_size             = archive_header->pack_size();
    auto compression_type = archive_header->pack_mode().compression_type();

    merged_files.emplace_back(std::move(archive_header));

    // Unpack merged files.
    if (pack_size != 0U) {
      switch (compression_type) {
      case ArchivedPackMode::kCompressionNone:
        extract_store(in_file);
        break;

      case ArchivedPackMode::kCompressionNormal:
        extract_normal(in_file);
        break;

      default:
        report_unknown();
        break;
      }

      in_file->skip(pack_size);
    }
  }
}

/* ---------------------------------------------------------------------- */

static auto list_archive(InputBuffer* in_file) -> void {
  unsigned int total_pack   = 0;
  unsigned int total_unpack = 0;
  unsigned int total_files  = 0;
  unsigned int merge_size   = 0;

  std::println("Unpacked   Packed Time     Date        Attrib   Name");
  std::println("-------- -------- -------- ----------- -------- ----");

  for (;;) {
    auto archive_header = ArchivedFileHeader::from_buffer(in_file);
    if (!archive_header) {
      break;
    }

    uint8_t const attributes  = archive_header->attributes(); /* file protection modes */
    uint32_t      unpack_size = archive_header->unpack_size();
    pack_size                 = archive_header->pack_size();
    const auto& stamp         = archive_header->datestamp();

    total_pack += pack_size;
    total_unpack += unpack_size;
    total_files++;
    merge_size += unpack_size;

    std::print("{:8} ", unpack_size);
    if ((archive_header->flags() & 1) != 0) {
      std::print("     n/a ");
    } else {
      std::print("{:8} ", pack_size);
    }

    std::print("{:02}:{:02}:{:02} ", stamp.hour(), stamp.minute(), stamp.second());
    std::print("{:2}-{}-{:4} ", stamp.day(), month_str[stamp.month()], stamp.year());

    std::print("{}{}{}{}{}{}{}{} ", ((attributes & 32) != 0) ? 'h' : '-',
        ((attributes & 64) != 0) ? 's' : '-', ((attributes & 128) != 0) ? 'p' : '-',
        ((attributes & 16) != 0) ? 'a' : '-', ((attributes & 1) != 0) ? 'r' : '-',
        ((attributes & 2) != 0) ? 'w' : '-', ((attributes & 8) != 0) ? 'e' : '-',
        ((attributes & 4) != 0) ? 'd' : '-');
    std::println("\"{}\"", archive_header->filename());
    if (!archive_header->comment().empty()) {
      std::println(": \"{}\"", archive_header->comment());
    }
    if (((archive_header->flags() & 1) != 0) && (pack_size != 0U)) {
      std::println("{:8} {:8} Merged", merge_size, pack_size);
    }

    if (pack_size != 0U) { /* seek past the packed data */
      merge_size = 0;
      in_file->skip(pack_size);
    }
  }

  std::println("-------- -------- -------- ----------- -------- ----");
  std::print("{:8} {:8} ", total_unpack, total_pack);
  std::println("{} file{}", total_files, ((total_files == 1) ? "" : "s"));
}

auto process_archive(char* filename, Action action) -> void {
  auto mmap_buffer = MmapInputBuffer::for_file(filename);
  auto in_buffer   = mmap_buffer->get();

  std::unique_ptr<FILE, decltype(&std::fclose)> in_file(std::fopen(filename, "rbe"), &std::fclose);
  if (in_file == nullptr) {
    throw std::runtime_error("could not open file");
  }

  in_buffer.read_into(&info_header, sizeof(info_header));
  fseek(in_file.get(), sizeof(info_header), SEEK_CUR);

  if ((info_header[0] != 'L') || (info_header[1] != 'Z') || (info_header[2] != 'X')) {
    throw std::runtime_error("not an LZX file");
  }

  switch (action) {
  case Action::Extract:
    extract_archive(&in_buffer);
    break;

  case Action::View:
    list_archive(&in_buffer);
    break;
  }
}

/* ---------------------------------------------------------------------- */

/* Some info for the reader only. This is unused by the program and can   */
/* safely be deleted.                                                     */

enum : uint8_t { INFO_DAMAGE_PROTECT = 1, INFO_FLAG_LOCKED = 2 };

/* STRUCTURE Info_Header
{
  UBYTE ID[3]; 0 - "LZX"
  UBYTE flags; 3 - INFO_FLAG_#?
  UBYTE[6]; 4
     } *//* SIZE = 10 */

enum : uint8_t { HDR_FLAG_MERGED = 1 };

enum : uint8_t {
  HDR_PROT_READ    = 1,
  HDR_PROT_WRITE   = 2,
  HDR_PROT_DELETE  = 4,
  HDR_PROT_EXECUTE = 8,
  HDR_PROT_ARCHIVE = 16,
  HDR_PROT_HOLD    = 32,
  HDR_PROT_SCRIPT  = 64,
  HDR_PROT_PURE    = 128
};

enum : uint8_t {
  HDR_TYPE_MSDOS   = 0,
  HDR_TYPE_WINDOWS = 1,
  HDR_TYPE_OS2     = 2,
  HDR_TYPE_AMIGA   = 10,
  HDR_TYPE_UNIX    = 20
};

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
static unsigned int unpack_size;

static std::deque<std::unique_ptr<ArchivedFileHeader>> merged_files;

/* ---------------------------------------------------------------------- */

static uint8_t read_buffer[16384]; /* have a reasonable sized read buffer */

uint8_t  decrunch_buffer[258 + 65536 + 258]; /* allow overrun for speed */
uint8_t* source;
uint8_t* destination;
uint8_t* source_end;
uint8_t* destination_end;

/* ---------------------------------------------------------------------- */

static const char* month_str[16] = {"?00", "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug",
    "sep", "oct", "nov", "dec", "?13", "?14", "?15"};

/* Opens a file for writing & creates the full path if required. */
static auto open_output(const char* filename) -> FILE* {
  FILE* file = nullptr;

  if ((file = fopen(filename, "wbe")) == nullptr) {
    // Compute the name of the encompassing directory and create it.
    // This logic assumes the file could not be opened because the parent directory doesn't exist.
    std::string dirname(filename);
    auto        pos = dirname.rfind('/');
    if (pos != std::string::npos) {
      dirname.resize(pos);
      std::filesystem::create_directories(dirname);
    }

    if ((file = fopen(filename, "wbe")) == nullptr) {
      std::println("Failed to open file '{}'", filename);
    }
  }
  return (file);
}

/* ---------------------------------------------------------------------- */

auto ArchivedFileHeader::from_file(FILE* fh) -> std::unique_ptr<ArchivedFileHeader> {
  auto result = std::make_unique<ArchivedFileHeader>();

  size_t const read_len = fread(&result->metadata_, 1, sizeof(result->metadata_), fh);

  if (read_len == 0) {
    return {};
  }

  if (read_len != sizeof(result->metadata_)) {
    throw std::runtime_error("Could not read archive header.");
  }

  crc::reset();

  uint32_t const header_crc = result->header_crc();
  result->clear_header_crc();

  crc::calc(&result->metadata_, sizeof(result->metadata_));

  result->filename_.resize(result->filename_length());
  if (fread(result->filename_.data(), 1, result->filename_length(), fh)
      != result->filename_length()) {
    throw std::runtime_error("Could not read archived file name.");
  }

  result->comment_.resize(result->comment_length());
  if (result->comment_length() > 0) {
    if (fread(result->comment_.data(), 1, result->comment_length(), fh)
        != result->comment_length()) {
      throw std::runtime_error("Could not read archived file comment.");
    }
  }

  crc::calc(result->filename_.data(), result->filename_.size());
  crc::calc(result->comment_.data(), result->comment_.size());

  if (crc::sum() != header_crc) {
    throw std::runtime_error("File header checksum invalid.");
  }

  return result;
}

static auto extract_normal(FILE* in_file) -> bool {
  FILE*          out_file = nullptr;
  unsigned char* pos      = nullptr;
  unsigned char* temp     = nullptr;
  unsigned int   count    = 0;

  huffman::HuffmanDecoder decoder;

  unpack_size             = 0;
  int64_t decrunch_length = 0;

  source_end = (source = read_buffer + 16384) - 1024;
  pos = destination_end = destination = decrunch_buffer + 258 + 65536;

  while (!merged_files.empty()) {
    auto node = std::move(merged_files.front());
    merged_files.pop_front();

    std::print("Extracting \"{}\"...", node->filename());

    out_file = open_output(node->filename().data());

    crc::reset();

    unpack_size = node->unpack_size();

    while (unpack_size > 0) {
      if (pos == destination) {     /* time to fill the buffer? */
                                    /* check if we have enough data and read some if not */
        if (source >= source_end) { /* have we exhausted the current read buffer? */
          temp  = read_buffer;
          count = temp - source + 16384;
          if (count != 0U) {
            do { /* copy the remaining overrun to the start of the buffer */
              *temp++ = *source++;
            } while (--count != 0U);
          }
          source = read_buffer;
          count  = source - temp + 16384;

          count = std::min(pack_size, count); /* make sure we don't read too much */

          if (fread(temp, 1, count, in_file) != count) {
            std::println("");
            if (ferror(in_file) != 0) {
              perror("FRead(Data)");
            } else {
              std::println(stderr, "EOF: Data");
            }
            return false;
          }
          pack_size -= count;

          temp += count;
          if (source >= temp) {
            break; /* argh! no more data! */
          }
        }

        /* if(source >= source_end) */
        /* check if we need to read the tables */
        if (decrunch_length <= 0) {
          if (decoder.read_literal_table() != 0) {
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

        decoder.decrunch();

        decrunch_length -= (destination - temp);
      }

      /* calculate amount of data we can use before we need to fill the buffer again */
      count = destination - pos;
      count = std::min(count, unpack_size); /* take only what we need */

      crc::calc(pos, count);

      if (out_file != nullptr) { /* Write the data to the file */
        if (fwrite(pos, 1, count, out_file) != count) {
          perror("FWrite"); /* argh! write error */
          fclose(out_file);
          out_file = nullptr;
        }
      }
      unpack_size -= count;
      pos += count;
    }

    if (out_file != nullptr) {
      fclose(out_file);
      std::println(" crc {}", (node->data_crc() == crc::sum()) ? "good" : "bad");
    }
  } /* for */

  return true;
}

/* ---------------------------------------------------------------------- */

/* This is less complex than extract_normal. Almost decipherable. */

static auto extract_store(FILE* in_file) -> bool {
  FILE*        out_file = nullptr;
  unsigned int count    = 0;

  while (!merged_files.empty()) {
    auto node = std::move(merged_files.front());
    merged_files.pop_front();

    std::print("Storing \"{}\"...", node->filename());

    out_file = open_output(node->filename().data());

    crc::reset();

    unpack_size = node->unpack_size();
    unpack_size = std::min(unpack_size, pack_size);

    while (unpack_size > 0) {
      count = (unpack_size > 16384) ? 16384 : unpack_size;

      if (fread(read_buffer, 1, count, in_file) != count) {
        std::println("");
        if (ferror(in_file) != 0) {
          perror("FRead(Data)");
        } else {
          std::println(stderr, "EOF: Data");
        }
        return false;
      }
      pack_size -= count;

      crc::calc(read_buffer, count);

      if (out_file != nullptr) { /* Write the data to the file */
        if (fwrite(read_buffer, 1, count, out_file) != count) {
          perror("FWrite"); /* argh! write error */
          fclose(out_file);
          out_file = nullptr;
        }
      }
      unpack_size -= count;
    }

    if (out_file != nullptr) {
      fclose(out_file);
      std::println(" crc {}", (node->data_crc() == crc::sum()) ? "good" : "bad");
    }
  } /* for */

  return true;
}

/* ---------------------------------------------------------------------- */

/* Easiest of the three. Just print the file(s) we didn't understand. */

static auto extract_unknown(FILE* /*in_file*/) -> bool {
  while (!merged_files.empty()) {
    auto node = std::move(merged_files.front());
    merged_files.pop_front();
    std::println("Skipping \"{}\": unknown compression mode.", node->filename());
  }

  return true;
}

/* ---------------------------------------------------------------------- */

/* Read the archive and build a linked list of names. Merged files is     */
/* always assumed. Will fail if there is no memory for a node. Sigh.      */

static auto extract_archive(FILE* in_file) -> bool {
  for (;;) {
    auto archive_header = ArchivedFileHeader::from_file(in_file);
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
        if (!extract_store(in_file)) {
          return false;
        }
        break;

      case ArchivedPackMode::kCompressionNormal:
        if (!extract_normal(in_file)) {
          return false;
        }
        break;

      default:
        if (!extract_unknown(in_file)) {
          return false;
        }
        break;
      }

      // Advance to the next file if we couldn't read the current one (pack_size is non-zero).
      if (fseek(in_file, pack_size, SEEK_CUR) != 0) {
        perror("FSeek(Data)");
        break;
      }
    }
  }

  return true;
}

/* ---------------------------------------------------------------------- */

/* List the contents of an archive in a nice formatted kinda way. */

static auto list_archive(FILE* in_file) -> bool {
  unsigned int const temp         = 0;
  unsigned int       total_pack   = 0;
  unsigned int       total_unpack = 0;
  unsigned int       total_files  = 0;
  unsigned int       merge_size   = 0;

  std::println("Unpacked   Packed Time     Date        Attrib   Name");
  std::println("-------- -------- -------- ----------- -------- ----");

  for (;;) {
    auto archive_header = ArchivedFileHeader::from_file(in_file);
    if (!archive_header) {
      break;
    }

    uint8_t const attributes = archive_header->attributes(); /* file protection modes */
    unpack_size              = archive_header->unpack_size();
    pack_size                = archive_header->pack_size();
    const auto& stamp        = archive_header->datestamp();

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
      if (fseek(in_file, pack_size, SEEK_CUR) != 0) {
        perror("FSeek(Data)");
        break;
      }
    }
  }

  std::println("-------- -------- -------- ----------- -------- ----");
  std::print("{:8} {:8} ", total_unpack, total_pack);
  std::println("{} file{}", total_files, ((total_files == 1) ? "" : "s"));

  return true;
}

/* ---------------------------------------------------------------------- */

/* Process a single archive. */

auto process_archive(char* filename, Action action) -> int {
  int   result  = 1; /* assume an error */
  FILE* in_file = nullptr;
  int   actual  = 0;

  if (nullptr == (in_file = fopen(filename, "rbe"))) {
    perror("FOpen(Archive)");
    return (result);
  }

  actual = fread(info_header, 1, 10, in_file);
  if (ferror(in_file) == 0) {
    if (actual == 10) {
      if ((info_header[0] == 76) && (info_header[1] == 90) && (info_header[2] == 88)) { /* LZX */
        switch (action) {
        case Action::Extract:
          result = extract_archive(in_file) ? 0 : 1;
          break;

        case Action::View:
          result = list_archive(in_file) ? 0 : 1;
          break;
        }
      } else {
        std::println(stderr, "Info_Header: Bad ID");
      }
    } else {
      std::println(stderr, "EOF: Info_Header");
    }
  } else {
    perror("FRead(Info_Header)");
  }
  fclose(in_file);

  return (result);
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

#include "lzx_handle.hh"

#include <format>
#include <iostream>

#include "crc.hh"

namespace lzx {

auto Entry::from_buffer(InputBuffer* buffer) -> std::unique_ptr<Entry> {
  if (buffer->is_eof()) return {};

  auto result = std::make_unique<Entry>();
  buffer->read_into(&result->metadata_, sizeof(result->metadata_));

  uint32_t const header_crc     = result->metadata_.header_crc_.value();
  result->metadata_.header_crc_ = 0;
  result->filename_             = buffer->read_string_view(result->metadata_.filename_length_);
  result->comment_              = buffer->read_string_view(result->metadata_.comment_length_);

  crc::reset();
  crc::calc(&result->metadata_, sizeof(result->metadata_));
  crc::calc(result->filename_.data(), result->filename_.size());
  crc::calc(result->comment_.data(), result->comment_.size());
  if (crc::sum() != header_crc) {
    throw std::runtime_error("File header checksum invalid.");
  }

  return result;
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
  HDR_TYPE_MSDOS   = 0,
  HDR_TYPE_WINDOWS = 1,
  HDR_TYPE_OS2     = 2,
  HDR_TYPE_AMIGA   = 10,
  HDR_TYPE_UNIX    = 20
};

}  // namespace lzx
#include "lzx_handle.hh"

#include "crc.hh"

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

std::string ArchivedFileHeader::attributes_str() const {
  char attrs[9]  = "hsparwed";
  char result[9] = "--------";

  for (int i = 0; i < 8; i++) {
    if ((attributes() & (1 << i)) != 0) {
      result[7 - i] = attrs[7 - i];
    }
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

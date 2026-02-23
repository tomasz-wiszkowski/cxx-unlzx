#include "lzx_entry.hh"

LzxFileSegment::LzxFileSegment(std::shared_ptr<LzxBlock> block, size_t decompressed_offset, size_t decompressed_length)
    : block_(std::move(block)), decompressed_offset_(decompressed_offset), decompressed_length_(decompressed_length) {}

std::shared_ptr<LzxBlock> LzxFileSegment::block() const {
  return block_;
}

std::span<const uint8_t> LzxFileSegment::data() const {
  if (auto data = block_->data()) {
    return data->subspan(decompressed_offset_, decompressed_length_);
  }
  return {};
}

Status LzxFileSegment::status() const {
  return block_->status();
}

size_t LzxFileSegment::decompressed_length() const {
  return decompressed_length_;
}

LzxEntry::LzxEntry(std::string name, lzx::Entry metadata, std::vector<LzxFileSegment> segments)
    : name_(std::move(name)), metadata_(std::move(metadata)), segments_(std::move(segments)) {}

const std::string& LzxEntry::name() const {
  return name_;
}

const lzx::Entry& LzxEntry::metadata() const {
  return metadata_;
}

const std::vector<LzxFileSegment>& LzxEntry::segments() const {
  return segments_;
}

std::optional<size_t> LzxEntry::pack_size() const {
  if (metadata_.flags().is_merged()) {
    size_t total_guessed = 0;
    for (const auto& segment : segments_) {
      if (segment.block()) {
        size_t block_unpacked = segment.block()->total_unpacked_size();
        if (block_unpacked > 0) {
          double ratio = static_cast<double>(segment.decompressed_length()) / block_unpacked;
          total_guessed += static_cast<size_t>(ratio * segment.block()->packed_size());
        }
      }
    }
    return total_guessed;
  }
  return metadata_.pack_size();
}

size_t LzxEntry::unpack_size() const {
  size_t total = 0;
  for (const auto& segment : segments_) {
    total += segment.decompressed_length();
  }
  return total;
}

lzx::DateStamp LzxEntry::datestamp() const {
  return metadata_.datestamp();
}

lzx::ProtectionBits LzxEntry::attributes() const {
  return metadata_.attributes();
}

const std::string& LzxEntry::comment() const {
  return metadata_.comment();
}

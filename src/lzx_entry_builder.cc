#include "lzx_entry_builder.hh"

LzxEntryBuilder::LzxEntryBuilder(lzx::Entry metadata)
    : metadata_(std::move(metadata)) {}

void LzxEntryBuilder::add_segment(LzxFileSegment segment) {
  segments_.push_back(std::move(segment));
}

void LzxEntryBuilder::add_segment(std::shared_ptr<LzxBlock> block, size_t decompressed_offset, size_t decompressed_length) {
  segments_.emplace_back(std::move(block), decompressed_offset, decompressed_length);
}

LzxEntry LzxEntryBuilder::build(std::string name) {
  return LzxEntry(std::move(name), std::move(metadata_), std::move(segments_));
}

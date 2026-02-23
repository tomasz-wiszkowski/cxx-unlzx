#include "lzx_block.hh"

#include <algorithm>
#include <cstring>

#include "circular_buffer.hh"
#include "huffman_decoder.hh"

std::optional<std::span<const uint8_t>> LzxBlock::data() {
  if (is_decompressed_) {
    if (status_ == Status::Ok) return data_span_;
    return std::nullopt;
  }
  is_decompressed_ = true;

  auto compression_type = node_.compression_info().mode();
  if (compression_type == lzx::CompressionInfo::Mode::kNone) {
    InputBuffer sub = in_file_;
    status_ = sub.read_span(total_unpacked_size_, data_span_);
    if (status_ == Status::Ok) {
      return data_span_;
    }
    return std::nullopt;
  } else if (compression_type == lzx::CompressionInfo::Mode::kNormal) {
    huffman::HuffmanDecoder decoder;
    decompressed_data_.emplace();
    decompressed_data_->resize(total_unpacked_size_ + 256);

    InputBuffer in = in_file_;
    size_t decrunch_length = 0;
    size_t pos = 0;

    while (pos < total_unpacked_size_) {
      if (decrunch_length <= 0) {
        status_ = decoder.read_literal_table(&in);
        if (status_ != Status::Ok) return std::nullopt;
        decrunch_length = decoder.decrunch_length();
      }

      size_t target_size = pos + decrunch_length;
      if (target_size > total_unpacked_size_) {
        target_size = total_unpacked_size_;
      }

      size_t old_pos = pos;
      status_ = decoder.decrunch(&in, *decompressed_data_, pos, target_size);
      if (status_ != Status::Ok) return std::nullopt;

      size_t decoded_bytes = pos - old_pos;
      if (decoded_bytes == 0 && in.is_eof()) {
        status_ = Status::UnexpectedEof;
        return std::nullopt;
      }
      
      if (decoded_bytes > decrunch_length) {
        decrunch_length = 0;
      } else {
        decrunch_length -= decoded_bytes;
      }
    }
    status_ = Status::Ok;
    decompressed_data_->resize(total_unpacked_size_);
    data_span_ = std::span<const uint8_t>(decompressed_data_->data(), total_unpacked_size_);
    return data_span_;
  } else {
    status_ = Status::UnknownCompression;
    return std::nullopt;
  }
}
#pragma once

#include <cstddef>
#include <print>
#include <span>
#include <stdexcept>
#include <vector>

template <typename T>
class CircularBuffer {
 private:
  void reset() {
    head = 0;
    tail = 0;
    full = false;
  }

 public:
  explicit CircularBuffer(size_t capacity) : buffer(capacity), buffer_full_threshold(capacity) {
    reset();
  }

  void push(const T& value) {
    if (is_full()) {
      throw std::runtime_error("Buffer overflow");
    }
    buffer[tail] = value;
    tail         = (tail + 1) % buffer.size();
    full         = tail == head;
  }

  void push(T&& value) {
    if (is_full()) {
      throw std::runtime_error("Buffer overflow");
    }
    buffer[tail] = std::move(value);
    tail         = (tail + 1) % buffer.size();
    full         = tail == head;
  }

  T pop() {
    if (is_empty()) {
      throw std::runtime_error("Buffer is empty");
    }
    T value = std::move(buffer[head]);
    head    = (head + 1) % buffer.size();
    full    = false;
    return value;
  }

  bool is_empty() const {
    return !full && (head == tail);
  }

  void set_fill_threshold(size_t threshold) {
    if (threshold > buffer.size()) {
      throw std::runtime_error("Threshold exceeds buffer size");
    }
    buffer_full_threshold = threshold;
  }

  bool fill_threshold_reached() const {
    return size() >= buffer_full_threshold;
  }

  bool is_full() const {
    return full;
  }

  size_t size() const {
    if (full) {
      return buffer.size();
    }
    if (tail >= head) {
      return tail - head;
    }
    return buffer.size() - head + tail;
  }

  size_t capacity() const {
    return buffer.size();
  }

  T& operator[](int32_t index) {
    return buffer[(head + index) % buffer.size()];
  }

  std::vector<std::span<T>> read(size_t count) {
    std::vector<std::span<T>> spans;

    if (tail < head) {
      size_t span_size = std::min(buffer.size() - head, count);
      spans.emplace_back(buffer.begin() + head, buffer.begin() + head + span_size);
      consume(span_size);
      count -= span_size;
    }

    if (count > 0) {
      size_t span_size = std::min(tail - head, count);
      spans.emplace_back(buffer.begin() + head, buffer.begin() + head + span_size);
      consume(span_size);
      count -= span_size;
    }

    return spans;
  }

  std::vector<std::span<T>> spans() {
    std::vector<std::span<T>> spans;
    if (is_empty()) {
      // No spans.
    } else if (tail > head) {
      spans.emplace_back(buffer.begin() + head, buffer.begin() + tail);
    } else {
      spans.emplace_back(buffer.begin() + head, buffer.end());
      spans.emplace_back(buffer.begin(), buffer.begin() + tail);
    }
    return spans;
  }

  void consume(size_t bytes) {
    if (bytes > size()) {
      throw std::runtime_error("Buffer underflow");
    }
    head = (head + bytes) % buffer.size();
    full = false;
  }

  void repeat(int offset, size_t count) {
    if (offset > buffer.size()) {
      throw std::runtime_error("Buffer underflow");
    }

    int32_t copy_from = (buffer.size() + tail - offset) % buffer.size();

    for (size_t i = 0; i < count; i++) {
      push(buffer[copy_from]);
      copy_from = (copy_from + 1) % buffer.size();
    }
  }

 private:
  std::vector<T> buffer;
  size_t         buffer_full_threshold;
  size_t         head;
  size_t         tail;
  bool           full;
};
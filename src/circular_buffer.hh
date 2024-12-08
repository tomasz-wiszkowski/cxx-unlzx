#pragma once

#include <cstddef>
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
  explicit CircularBuffer(size_t capacity) : buffer(capacity) {
    reset();
  }

  template <typename U>
  void push(U&& value) {
    if (is_full()) {
      throw std::runtime_error("Buffer overflow");
    }
    buffer[tail] = std::forward<U>(value);
    tail         = (tail + 1) % buffer.size();
    full         = tail == head;
  }

  T pop() {
    if (is_empty()) {
      throw std::runtime_error("Buffer underflow");
    }
    T value = std::move(buffer[head]);
    head    = (head + 1) % buffer.size();
    full    = false;
    return value;
  }

  bool is_empty() const {
    return !full && (head == tail);
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

  T& operator[](size_t index) {
    return buffer[(head + index) % buffer.size()];
  }

  std::vector<std::span<T>> read(size_t count) {
    std::vector<std::span<T>> res;
    if (count == 0) return res;

    res = spans();
    if (res.empty()) return res;

    size_t consume_length = std::min(count, res[0].size());
    res[0]                = res[0].subspan(0, consume_length);
    consume(consume_length);
    count -= consume_length;

    if (res.size() > 1) {
      size_t consume_length = std::min(count, res[1].size());
      res[1]                = res[1].subspan(0, consume_length);
      consume(consume_length);
      count -= consume_length;
      if (res[1].size() == 0) {
        res.pop_back();
      }
    }

    return res;
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

  void repeat(size_t offset, size_t count) {
    if (offset > buffer.size()) {
      throw std::runtime_error("Buffer underflow");
    }

    size_t copy_from = (buffer.size() + tail - offset) % buffer.size();

    for (size_t i = 0; i < count; i++) {
      push(buffer[copy_from]);
      copy_from = (copy_from + 1) % buffer.size();
    }
  }

 private:
  std::vector<T> buffer;
  size_t         head;
  size_t         tail;
  bool           full;
};
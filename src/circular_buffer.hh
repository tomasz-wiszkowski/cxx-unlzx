#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

template <typename T>
class CircularBuffer {
 public:
  explicit CircularBuffer(size_t capacity) : buffer(capacity), head(0), tail(0), full(false) {}

  void push(const T& value) {
    buffer[tail] = value;
    tail         = (tail + 1) % buffer.size();
    if (full) {
      head = (head + 1) % buffer.size();  // Overwrite the oldest element
    }
    full = tail == head;
  }

  T pop() {
    if (is_empty()) {
      throw std::runtime_error("Buffer is empty");
    }
    T value = buffer[head];
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

  std::vector<std::span<T>> spans() {
    std::vector<std::span<T>> spans;
    if (full) {
      spans.emplace_back(buffer);
    } else if (tail >= head) {
      spans.emplace_back(buffer.begin() + head, buffer.begin() + tail);
    } else {
      spans.emplace_back(buffer.begin() + head, buffer.end());
      spans.emplace_back(buffer.begin(), buffer.begin() + tail);
    }
    return spans;
  }

 private:
  std::vector<T> buffer;
  size_t         head;
  size_t         tail;
  bool           full;
};
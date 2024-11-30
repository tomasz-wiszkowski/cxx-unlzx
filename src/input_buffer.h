#include <cstdint>
#include <memory>

class InputBuffer {
 public:
  static std::unique_ptr<InputBuffer> for_file(const char* filepath);
  ~InputBuffer();

  // To be deleted.
  uint8_t read_byte();
  // To be deleted.
  uint16_t read_word();

  uint16_t read_bits(size_t data_bits_requested);

 private:
  int      fd_{};
  size_t   filesize_{};
  uint8_t* data_{};
  size_t   current_position_{};

  size_t data_bits_{};
  size_t data_bits_available_{};
};
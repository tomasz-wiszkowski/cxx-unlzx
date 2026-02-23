#include "error.hh"

std::string_view format_status(Status status) {
  switch (status) {
  case Status::Ok:
    return "Ok";
  case Status::BufferOverflow:
    return "Buffer overflow";
  case Status::BufferUnderflow:
    return "Buffer underflow";
  case Status::OutOfRange:
    return "Out of range";
  case Status::MisalignedData:
    return "Misaligned data";
  case Status::UnexpectedEof:
    return "Unexpected end of file";
  case Status::FileCreateError:
    return "File create error";
  case Status::FileWriteError:
    return "File write error";
  case Status::FileOpenError:
    return "File open error";
  case Status::NotLzxFile:
    return "Not an LZX file";
  case Status::FileMapError:
    return "File map error";
  case Status::ChecksumInvalid:
    return "Checksum invalid";
  case Status::HuffmanTableError:
    return "Huffman table error";
  case Status::UnknownCompression:
    return "Unknown compression mode";
  default:
    return "Unknown error";
  }
}

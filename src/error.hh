#pragma once

/**
 * @brief Represents the status of an operation.
 */
enum class Status {
  Ok = 0,               ///< Success.
  BufferOverflow,       ///< Attempted to write past buffer end.
  BufferUnderflow,      ///< Attempted to read past buffer end.
  OutOfRange,           ///< Index or value is out of range.
  MisalignedData,       ///< Data alignment requirements not met.
  UnexpectedEof,        ///< Unexpected end of file or stream.
  FileCreateError,      ///< Failed to create file.
  FileWriteError,       ///< Failed to write to file.
  FileOpenError,        ///< Failed to open file.
  NotLzxFile,           ///< Invalid LZX file header.
  FileMapError,         ///< Failed to memory map file.
  ChecksumInvalid,      ///< Data integrity check failed.
  HuffmanTableError,    ///< Huffman table construction or use error.
  UnknownCompression,   ///< Unsupported compression format.
};

/**
 * @brief Executes an expression and returns its Status if not Ok.
 * @param expr Expression returning a Status.
 */
#define TRY(expr)                                                                                  \
  do {                                                                                             \
    Status _status = (expr);                                                                       \
    if (_status != Status::Ok) {                                                                   \
      return _status;                                                                              \
    }                                                                                              \
  } while (0)

#ifndef _WIN32
#include <unistd.h>
#endif

#include <print>
#include <set>

#include "error.hh"
#include "unlzx.hh"

enum class Action : uint8_t { List, Extract };


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

auto main(int argc, char** argv) -> int {
  int    result = 0;
  Action action = Action::Extract;
  int    first_file;

#ifdef _WIN32
  first_file = 1;
  while (first_file < argc && argv[first_file][0] == '-') {
    for (int j = 1; argv[first_file][j] != '\0'; ++j) {
      switch (argv[first_file][j]) {
      case 'l':  // (l)ist archive
        action = Action::List;
        break;
      case 'x':  // e(x)tract archive
        action = Action::Extract;
        break;
      default:
        result = 1;
        break;
      }
    }
    ++first_file;
  }
#else
  while (true) {
    int option = getopt(argc, argv, "lx");
    if (option == -1) {
      break;
    }

    switch (option) {
    case 'l':  // (l)ist archive
      action = Action::List;
      break;
    case 'x':  // e(x)tract archive
      action = Action::Extract;
      break;
    case '?':  // unknown option
    default:
      result = 1;
      break;
    }
  }
  first_file = optind;
#endif

  if (argc - first_file != 1) {
    std::println("Usage: unlzx [-l][-x] archive");
    std::println("\t-l : list archive");
    std::println("\t-x : extract (default)");
    return 2;
  }

  {
    std::println("Archive \"{}\"...", argv[first_file]);
    Unlzx  unlzx;
    Status status = unlzx.open_archive(argv[first_file]);
    if (status != Status::Ok) {
      std::println("Error processing archive \"{}\": {}", argv[first_file], format_status(status));
      return 1;
    }

    if (action == Action::List) {
      auto entries = unlzx.list_archive();

      size_t total_unpack = 0;
      size_t total_files  = 0;

      std::println("Unpacked Packed   Time     Date       Attrib   Name");
      std::println("-------- -------- -------- ---------- -------- ----");

      for (const auto& [name, entry] : entries) {
        total_unpack += entry.unpack_size();
        total_files  += entry.segments().size();

        std::print("{:8} ", entry.unpack_size());
        if (auto pack = entry.pack_size()) {
          std::print("{:8} ", *pack);
        } else {
          std::print("     n/a ");
        }
        std::print("{0:t} {0:d} {1} ", entry.datestamp(), entry.attributes());

        std::println("\"{}\"", name);
        if (!entry.comment().empty()) {
          std::println(": \"{}\"", entry.comment());
        }
      }

      std::println("-------- -------- -------- ---------- -------- ----");
      std::print("{:8}      n/a ", total_unpack);
      std::println("{} file{}", total_files, ((total_files == 1) ? "" : "s"));
    } else {
      status = unlzx.extract_archive();
      if (status != Status::Ok) {
        std::println("Error extracting archive \"{}\": {}", argv[first_file], format_status(status));
      }
    }
  }
  return 0;
}

#ifndef _WIN32
#include <unistd.h>
#endif

#include <filesystem>
#include <fstream>
#include <print>
#include <regex>
#include <set>
#include <span>
#include <vector>

#include "crc.hh"
#include "error.hh"
#include "unlzx.hh"

enum class Action : uint8_t { List, Extract, View };


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

class FilteredEntries {
public:
  class Iterator {
  public:
    Iterator(std::map<std::string, LzxEntry>::const_iterator it,
             std::map<std::string, LzxEntry>::const_iterator end,
             const FilteredEntries* parent)
        : it_(it), end_(end), parent_(parent) {
      advance_to_match();
    }

    Iterator& operator++() {
      ++it_;
      advance_to_match();
      return *this;
    }

    bool operator!=(const Iterator& other) const { return it_ != other.it_; }
    const std::pair<const std::string, LzxEntry>& operator*() const { return *it_; }

  private:
    void advance_to_match() {
      while (it_ != end_ && !parent_->matches(it_->first)) {
        ++it_;
      }
    }

    std::map<std::string, LzxEntry>::const_iterator it_;
    std::map<std::string, LzxEntry>::const_iterator end_;
    const FilteredEntries* parent_;
  };

  FilteredEntries(const std::map<std::string, LzxEntry>& entries, std::span<char* const> names, bool use_regex)
      : entries_(entries), names_(names), use_regex_(use_regex) {
    if (use_regex) {
      for (const char* p : names) {
        res_.emplace_back(p, std::regex::ECMAScript | std::regex::icase);
      }
    }
  }

  bool matches(const std::string& name) const {
    if (names_.empty()) {
      return true;
    }
    for (size_t i = 0; i < names_.size(); ++i) {
      if (use_regex_) {
        if (std::regex_search(name, res_[i])) {
          return true;
        }
      } else {
        if (name == names_[i]) {
          return true;
        }
      }
    }
    return false;
  }

  Iterator begin() const { return Iterator(entries_.begin(), entries_.end(), this); }
  Iterator end() const { return Iterator(entries_.end(), entries_.end(), this); }

private:
  const std::map<std::string, LzxEntry>& entries_;
  std::span<char* const> names_;
  bool use_regex_;
  std::vector<std::regex> res_;
};

int handle_list(const FilteredEntries& entries) {
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
  return 0;
}

int handle_view(const FilteredEntries& entries) {
  int matched_files = 0;

  for (const auto& [name, entry] : entries) {
    std::println("-- contents of {}", name);
    for (const auto& segment : entry.segments()) {
      auto data = segment.data();
      std::print("{}", std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
    }
    matched_files++;
  }

  std::println("-- {} files matched", matched_files);
  return 0;
}

int handle_extract(const FilteredEntries& entries) {
  for (const auto& [name, entry] : entries) {
    const auto& path = entry.path();

    if (entry.unpack_size() == 0 && name.ends_with("/")) {
      std::filesystem::create_directories(path);
      std::print("Creating directory \"{}\"", name);
      std::println(" crc good");
      continue;
    }

    if (path.has_parent_path()) {
      std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out_file(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out_file) {
      std::println("Error creating file \"{}\"", name);
      return 1;
    }

    std::print("Writing \"{}\"...", name);
    crc::Crc32 crc_calc;
    bool       error = false;
    for (const auto& segment : entry.segments()) {
      auto data = segment.data();

      if (segment.status() != Status::Ok) {
        std::println(" error: {}", format_status(segment.status()));
        error = true;
        break;
      }

      crc_calc.calc(data.data(), data.size());
      if (!out_file.write(reinterpret_cast<const char*>(data.data()), data.size())) {
        std::println(" error writing file");
        error = true;
        break;
      }
    }

    if (error) continue;

    std::println(" crc {}", (entry.metadata().data_crc() == crc_calc.sum()) ? "good" : "bad");
  }
  return 0;
}

auto main(int argc, char** argv) -> int {
  int    result = 0;
  Action action = Action::Extract;
  bool   use_regex = false;

  int flag_idx = 1;
  while (flag_idx < argc && std::string_view(argv[flag_idx]) == "--regex") {
    use_regex = true;
    ++flag_idx;
  }

  int shift = flag_idx - 1;
  if (shift > 0) {
    for (int i = 1; i < argc - shift; ++i) {
      argv[i] = argv[i + shift];
    }
    argc -= shift;
    argv[argc] = nullptr;
  }

  int first_file;

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
      case 'v':  // (v)iew file in archive
        action = Action::View;
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
    int option = getopt(argc, argv, "lxv");
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
    case 'v':  // (v)iew file in archive
      action = Action::View;
      break;
    case '?':  // unknown option
    default:
      result = 1;
      break;
    }
  }
  first_file = optind;
#endif

  if (argc - first_file < 1) {
    std::println("Usage: unlzx [--regex] [-l][-x][-v] archive [file...]");
    std::println("\t--regex : treat file(s) as regex patterns (with -l, -v)");
    std::println("\t-l : list archive");
    std::println("\t-x : extract (default)");
    std::println("\t-v : view file(s) in archive");
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

    auto map_entries = unlzx.list_archive();
    FilteredEntries entries(map_entries, std::span<char* const>(argv + first_file + 1, static_cast<size_t>(argc - (first_file + 1))), use_regex);

    if (action == Action::List) {
      return handle_list(entries);
    } else if (action == Action::View) {
      return handle_view(entries);
    } else {
      return handle_extract(entries);
    }
  }
  return 0;
}

#ifndef _WIN32
#include <unistd.h>
#endif

#include <print>

#include "unlzx.hh"

auto main(int argc, char** argv) -> int {
  int    result = 0;
  Action action = Action::Extract;
  int    first_file;

#ifdef _WIN32
  first_file = 1;
  while (first_file < argc && argv[first_file][0] == '-') {
    for (int j = 1; argv[first_file][j] != '\0'; ++j) {
      switch (argv[first_file][j]) {
      case 'v':  // (v)iew archive
        action = Action::View;
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
    int option = getopt(argc, argv, "vx");
    if (option == -1) {
      break;
    }

    switch (option) {
    case 'v':  // (v)iew archive
      action = Action::View;
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

  if (first_file >= argc) {
    std::println("Usage: unlzx [-v][-x][-c] [archive...]");
    std::println("\t-v : list archive(s)");
    std::println("\t-x : extract (default)");
    return 2;
  }

  for (; first_file < argc; ++first_file) {
    std::println("Archive \"{}\"...", argv[first_file]);
    try {
      process_archive(argv[first_file], action);
    } catch (std::exception& e) {
      std::println("Error processing archive \"{}\": {}", argv[first_file], e.what());
    }
  }
  return 0;
}

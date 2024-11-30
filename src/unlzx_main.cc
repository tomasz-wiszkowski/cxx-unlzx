#include <unistd.h>

#include <print>

#include "unlzx.h"

auto main(int argc, char** argv) -> int {
  int    result = 0;
  Action action = Action::Extract;

  while (true) {
    int option = getopt(argc, argv, "vxc");
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

  if (optind >= argc) {
    std::println("Usage: unlzx [-v][-x][-c] [archive...]");
    std::println("\t-v : list archive(s)");
    std::println("\t-x : extract (default)");
    return 2;
  }

  for (; optind < argc; optind++) {
    std::println("Archive \"{}\"...", argv[optind]);
    process_archive(argv[optind], action);
  }
  return 0;
}

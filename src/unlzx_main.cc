/* Handle options & multiple filenames. */
#include <stdio.h>
#include <unistd.h>

#include "unlzx.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char **argv) {
    int result = 0;
    int read_from_stdin = 0;
    int option;
    Action action = Action::Extract;

    while (true) {
		option = getopt(argc, argv, "vxc");
		if (option == -1) break;

        switch (option) {
            case 'v':  // (v)iew archive
                action = Action::View;
                break;
            case 'x':  // e(x)tract archive
                action = Action::Extract;
                break;
            case 'c':  // use stdin to extract/view from
                read_from_stdin = 1;
                break;
            case '?':  // unknown option
                result = 1;
                break;
        }
    }

    if (!read_from_stdin && optind >= argc) {
        result = 1;
    }

    if (result) {
        fprintf(stderr, "Usage: unlzx [-v][-x][-c] [archive...]\n");
        fprintf(stderr, "\t-c : extract/list from stdin\n");
        fprintf(stderr, "\t-v : list archive(s)\n");
        fprintf(stderr, "\t-x : extract (default)\n");
        return 2;
    }

    if (read_from_stdin) {
        printf("\nReading from stdin...\n\n");
        process_archive(nullptr, action);
        return 0;
    }

	for (; optind < argc; optind++) {
		printf("\nArchive \"%s\"...\n\n", argv[optind]);
		process_archive(argv[optind], action);
	}
	return 0;
}

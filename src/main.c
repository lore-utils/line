#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "lines.h"
#include "ranges.h"

static void print_help(void) {
    fprintf(stderr,
        "line - a line util to cleanly and sanely extract line(s)\n"
        "Usage:\n"
        "    line <OPTIONS> MARKER...\n"
        "Options:\n"
        "    -[-l]ine MARKER - line marker explicitly\n"
        "    -[-f]ile FILE - input file. defaults to \"-\" for stdin\n"
        "    -[-h]elp - this message\n");
}

//Line num is 1 indexed, nobody actually asks for line 0
int main(int argc, char ** argv) {
    if (argc == 1) {
        print_help();
        exit(EXIT_SUCCESS);
    }

    const char * file_name = "-";
    condition_set_t set;
    init_condition_set(&set);

    int c;
    while (1) {
        int option_index = 0;

        static struct option long_options[] = {{"help", no_argument, 0, 'h'},
            {"line", required_argument, 0, 'l'}, {"file", required_argument, 0, 'f'}, {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "hl:f:", long_options, &option_index);

        if (c == -1) {
            for (int i = optind; i < argc; ++i) {
                switch (arg_type(argv[i])) {
                    case 's':
                    case 'l':
                        extract_list(argv[i], &set);
                        break;
                    case 'r':
                        extract_range(argv[i], &set);
                        break;
                    case '\0':
                    default:
                        fprintf(stderr, "Unable to parse line argument '%s'\n", argv[i]);
                }
            }
            break;
        }

        switch (c) {
            case 'f':
                file_name = optarg;
                break;
            case 'l':
            case ':':
                switch (arg_type(optarg)) {
                    case 's':
                    case 'l':
                        extract_list(optarg, &set);
                        break;
                    case 'r':
                        extract_range(optarg, &set);
                        break;
                    case '\0':
                    default:
                        fprintf(stderr, "Unable to parse line argument '%s'\n", optarg);
                }
                break;
            case 'h':
            case '?':
                print_help();
                exit(EXIT_SUCCESS);
        }
    }
    if (set.size == 0) { //no conditions set
        print_help();
        exit(EXIT_SUCCESS);
    }

    int fd = STDIN_FILENO;
    if (file_name != NULL && strcmp(file_name, "-") != 0) {
        fd = open(file_name, O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            perror("open");
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "stdin not yet supported!\n");
        exit(EXIT_FAILURE);
    }


    qsort(set.conditions, set.size, sizeof(condition_t), condition_sort);
    //TODO eliminate any lines that fall inside of ranges. they will break everything

    //Same algorithm as C++ std::unique
    //May be worth doing a custom mergesort to do it in 1 iteration, plus inlining is nice
    file_get_lines(fd, &set);

    close_condition_set(&set);

    return EXIT_SUCCESS;
}

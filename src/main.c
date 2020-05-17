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
        "       range: <#>-<#>        - 4-10\n"
        "       list: <#>,<#>,<#>,... - 1,2,3\n"
        "       single: <#>           - 8\n"
        "    -[-f]ile FILE - input file. defaults to \"-\" for stdin\n"
        "    -[-h]elp - this message\n"
        "Examples:\n"
        "   line 1 2,5,7 440-1000\n"
        "   line -l 500-1000 -f /path/to/file\n"
        );
}

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
                        if (extract_list(argv[i], &set)) {
                            print_help();
                            exit(EXIT_FAILURE);
                        }
                        break;
                    case 'r':
                        if (extract_range(argv[i], &set)) {
                            print_help();
                            exit(EXIT_FAILURE);
                        }
                        break;
                    case '\0':
                    default:
                        fprintf(stderr, "Unable to parse line argument '%s'\n", argv[i]);
                        print_help();
                        exit(EXIT_FAILURE);
                        break;
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
                        if (extract_list(optarg, &set)) {
                            print_help();
                            exit(EXIT_FAILURE);
                        }
                        break;
                    case 'r':
                        if (extract_range(optarg, &set)) {
                            print_help();
                            exit(EXIT_FAILURE);
                        }
                        break;
                    case '\0':
                    default:
                        fprintf(stderr, "Unable to parse line argument '%s'\n", optarg);
                        print_help();
                        exit(EXIT_FAILURE);
                        break;
                }
                break;
            case 'h':
            case '?':
                print_help();
                exit(EXIT_SUCCESS);
        }
    }
    //no conditions set
    if (set.size == 0) {
        print_help();
        exit(EXIT_SUCCESS);
    }

    //make sure to actually start at the start of the options
    set.index = 0;

    qsort(set.conditions, set.size, sizeof(condition_t), condition_sort);

    consolidate_conditions(&set);

    int fd = STDIN_FILENO;
    if (strcmp(file_name, "-") != 0) {
        fd = open(file_name, O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        file_get_lines(fd, &set);
    } else {
        //stdin cant be mmaped :c
        buff_get_lines(fd, &set);
    }

    close_condition_set(&set);

    return EXIT_SUCCESS;
}

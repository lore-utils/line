#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>

static void print_help(void) {
    fprintf(stderr,
        "line - a line util to cleanly and sanely extract line(s)\n"
        "Usage:\n"
        "    line <OPTIONS>\n"
        "Options:\n"
        "    Lorum Ipsum\n"
        "\n"
        "    Dolor Sit Amet\n"
        "\n"
        "    -[-h]elp - this message\n"
        "Notes:\n"
        "    Lorem Ipsum.\n");
}

//Line num is 1 indexed, nobody actually asks for line 0
static void get_line(uint64_t line_num, const char * filename) {
    int fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    //32 pages was best from testing
    const size_t buffer_size = page_size * 32;

    unsigned char * restrict buffer = malloc(buffer_size);

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    madvise(buffer, buffer_size, MADV_SEQUENTIAL);

    uint64_t line_count = 1;

    while (true) {
        ssize_t ret = read(fd, buffer, buffer_size);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return;
        }
        if (ret == 0) {
            break;
        }
        unsigned char *read_start = buffer;
        size_t read_len = ret;

        while (true) {
            unsigned char *line_sep = memchr(read_start, '\n', read_len);
            size_t line_len;
            if (line_sep == NULL) {
                //No separator found
                if (line_count == line_num) {
                    line_len = read_len;
                    write(STDOUT_FILENO, read_start, line_len);
                }
                //Actually continue on the read loop, not the line loop
                break;
            } else {
                //Line len includes the \n
                line_len = (line_sep - read_start) + 1;
                if (line_count == line_num) {
                    write(STDOUT_FILENO, read_start, line_len);
                    //We only currently handle single line finding
                    return;
                }
                ++line_count;
                read_start += line_len;
                read_len -= line_len;
            }
        }
    }

    free(buffer);
    close(fd);
}

int main(int argc, char ** argv) {
    if (argc == 1) {
        print_help();
        exit(EXIT_SUCCESS);
    }

    uint64_t line_num = 0;

    const char * file_name = NULL;

    int c;
    while (1) {
        int option_index = 0;

        static struct option long_options[]
            = {{"help", no_argument, 0, 'h'}, {"lines", required_argument, 0, 'l'},
                {"file", required_argument, 0, 'f'}, {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "hl:f:", long_options, &option_index);

        if (c == -1) {
            for (int i = optind; (file_name == NULL && i < argc - 1) || (file_name != NULL && i < argc); ++i) {
                char * endptr = 0;
                line_num      = strtoul(argv[i], &endptr, 0);
                if (errno == ERANGE && line_num == ULLONG_MAX) {
                    fprintf(stderr, "Line numbers must be between 1 and %lu\n", ULONG_MAX);
                    print_help();
                }
            }
            //dont write to the file_name if it was already specified
            if (!file_name) {
                file_name = argv[argc - 1];
            }
            break;
        }

        switch (c) {
            case 'f':
                file_name = optarg;
                break;
            case 'l':
            case ':': {
                char * endptr = 0;
                line_num      = strtoul(optarg, &endptr, 0);
                if (errno == ERANGE && line_num == ULLONG_MAX) {
                    fprintf(stderr, "Line numbers must be between 1 and %lu\n", ULONG_MAX);
                    print_help();
                }
                break;
            }
            case 'h':
            case '?':
                print_help();
                exit(EXIT_SUCCESS);
        }
    }
    if (line_num == 0 || file_name == NULL) {
        fprintf(stderr, "You need to specify both a line number and a file name\n");
        print_help();
        exit(EXIT_SUCCESS);
    }

    get_line(line_num, file_name);

    return EXIT_SUCCESS;
}

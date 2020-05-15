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
        "    line <OPTIONS> MARKER...\n"
        "Options:\n"
        "    -[l]ine MARKER - line marker explicitly\n"
        "    -[f]ile FILE - input file. defaults to \"-\" for stdin\n"
        "    -[-h]elp - this message\n");
}

//Line num is 1 indexed, nobody actually asks for line 0
static void get_line(uint64_t line_num, int fd) {
    struct stat res;
    if (fstat(fd, &res) == -1) {
        perror("fstat");
        return;
    }
    const size_t file_size = res.st_size;

    unsigned char * restrict buffer = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);

    madvise(buffer, file_size, MADV_SEQUENTIAL);

    uint64_t line_count = 1;

    unsigned char *read_start = buffer;
    size_t read_len = file_size;

    while (true) {
        unsigned char *line_sep = memchr(read_start, '\n', read_len);
        size_t line_len;
        if (line_sep == NULL) {
            //No separator found
            if (line_count == line_num) {
                line_len = read_len;
                write(STDOUT_FILENO, read_start, line_len);
            }
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

    munmap(buffer, file_size);
    close(fd);
}

int main(int argc, char ** argv) {
    if (argc == 1) {
        print_help();
        exit(EXIT_SUCCESS);
    }

    uint64_t line_num = 0;

    const char * file_name = "-";

    int c;
    while (1) {
        int option_index = 0;

        static struct option long_options[] = {{"help", no_argument, 0, 'h'},
            {"line", required_argument, 0, 'l'}, {"file", required_argument, 0, 'f'}, {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "hl:f:", long_options, &option_index);

        if (c == -1) {
            for (int i = optind; i < argc; ++i) {
                char * endptr = 0;
                line_num      = strtoull(argv[i], &endptr, 0);
                if (errno == ERANGE && line_num == ULLONG_MAX || errno == EINVAL) {
                    fprintf(stderr, "Line numbers must be between 1 and %lu\n", ULONG_MAX);
                    print_help();
                }
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
                line_num      = strtoull(optarg, &endptr, 0);
                if (errno == ERANGE && line_num == ULLONG_MAX || errno == EINVAL) {
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
    if (line_num == 0) {
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
    }
    get_line(line_num, fd);

    return EXIT_SUCCESS;
}

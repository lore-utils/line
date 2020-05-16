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
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

int int_cmp(const void *lhs, const void *rhs) {
    const uint64_t l = *(const uint64_t *) lhs;
    const uint64_t r = *(const uint64_t *) rhs;
    return l > r;
}

//Line num is 1 indexed, nobody actually asks for line 0
static void get_line(int fd, const uint64_t *restrict line_numbers, size_t target_count) {
    struct stat res;
    if (fstat(fd, &res) == -1) {
        perror("fstat");
        return;
    }
    const size_t file_size = res.st_size;

    /*
     * We originally implemented this using read() calls reading 32 pages at a time.
     * mmap was significantly faster, but in an unconventional way.
     * The total cycle count significantly increased (10-20% by our tests).
     * But the machines stayed at a higher clock speed, which more than offset the difference.
     */
    unsigned char *restrict buffer
            = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);

    madvise(buffer, file_size, MADV_SEQUENTIAL);

    uint64_t line_count = 1;

    size_t target_idx = 0;

    unsigned char *read_start = buffer;
    size_t read_len = file_size;

    while (true) {
        unsigned char *line_sep = memchr(read_start, '\n', read_len);
        size_t line_len;
        if (line_sep == NULL) {
            //No separator found
            if (line_count == line_numbers[target_idx]) {
                line_len = read_len;
                write(STDOUT_FILENO, read_start, line_len);
            }
            //line_count is 1 greater than the total due to loop iteration, so we use <= instead of <
            if (line_count <= line_numbers[target_count - 1]) {
                //We reached EOF before fulfilling all requested lines
                printf("The file isn't that long!\n");
                //Exit out of all processing
                break;
            }
            break;
        } else {
            //Line len includes the \n
            line_len = (line_sep - read_start) + 1;
            if (line_count == line_numbers[target_idx]) {
                write(STDOUT_FILENO, read_start, line_len);
                ++target_idx;
                if (target_idx >= target_count) {
                    return;
                }
            }
            ++line_count;
            read_start += line_len;
            read_len -= line_len;
        }
    }

    munmap(buffer, file_size);
    close(fd);
}

int main(int argc, char **argv) {
    if (argc == 1) {
        print_help();
        exit(EXIT_SUCCESS);
    }

    size_t buffer_size = (argc - 1);
    uint64_t *restrict line_numbers = malloc(sizeof(uint64_t) * buffer_size);
    size_t line_count = 0;

    uint64_t line_num = 0;

    const char *file_name = "-";

    int c;
    while (1) {
        int option_index = 0;

        static struct option long_options[]
                = {{"help", no_argument, 0, 'h'}, {"line", required_argument, 0, 'l'},
                        {"file", required_argument, 0, 'f'}, {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "hl:f:", long_options, &option_index);

        if (c == -1) {
            for (int i = optind; i < argc; ++i) {
                char *endptr = NULL;
                line_num = strtoull(argv[i], &endptr, 0);
                if (line_num == 0) {
                    fprintf(stderr, "Line numbers must be valid integers\n");
                    print_help();
                    exit(EXIT_SUCCESS);
                }
                if ((errno == ERANGE && line_num == ULLONG_MAX) || errno == EINVAL) {
                    fprintf(stderr, "Line numbers must be between 1 and %llu\n", ULLONG_MAX);
                    print_help();
                    exit(EXIT_SUCCESS);
                }
                if (endptr && *endptr == '-') {
                    uint64_t range_end = strtoull(endptr + 1, NULL, 0);
                    if (range_end == 0 || range_end < line_num) {
                        fprintf(stderr, "Invalid line range!\n");
                        print_help();
                        exit(EXIT_SUCCESS);
                    }
                    if ((errno == ERANGE && range_end == ULLONG_MAX) || errno == EINVAL) {
                        fprintf(stderr, "Line numbers must be between 1 and %llu\n", ULLONG_MAX);
                        print_help();
                        exit(EXIT_SUCCESS);
                    }
                    /*
                     * This can theoretically be really stupid with bad input, need better solution
                     * And by better solution, I mean a dedicated range function
                     * On large files, not only can this get stupidly memory heavy, but it will also induce
                     * a shit ton of page faults, killing performance
                     * For example, on a 21M line file (~800MB), the difference between 1-max and 1 max
                     * is 8.3 seconds (0.2s vs 8.5s)
                     * It also jumps from 47 page faults to over 62k
                     * On the plus side, a range function would make output significantly faster
                     * since we know input is contiguous, and could do vmsplice or pwritev shenanigans
                     */
                    buffer_size += (range_end - line_num) + 1;
                    line_numbers = realloc(line_numbers, sizeof(uint64_t) * buffer_size);
                    for (uint64_t i = line_num; i <= range_end; ++i) {
                        line_numbers[line_count++] = i;
                    }
                } else {
                    line_numbers[line_count++] = line_num;
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
                char *endptr = 0;
                line_num = strtoull(optarg, &endptr, 0);
                if (line_num == 0) {
                    fprintf(stderr, "Line numbers must be valid integers\n");
                    print_help();
                    exit(EXIT_SUCCESS);
                }
                if ((errno == ERANGE && line_num == ULLONG_MAX) || errno == EINVAL) {
                    fprintf(stderr, "Line numbers must be between 1 and %llu\n", ULLONG_MAX);
                    print_help();
                }
                line_numbers[line_count++] = line_num;
                break;
            }
            case 'h':
            case '?':
                print_help();
                exit(EXIT_SUCCESS);
        }
    }
    if (line_count == 0) {
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

    qsort(line_numbers, line_count, sizeof(uint64_t), int_cmp);

    //Same algorithm as C++ std::unique
    //May be worth doing a custom mergesort to do it in 1 iteration, plus inlining is nice
    uint64_t *head = line_numbers;
    for (size_t i = 1; i < line_count; ++i) {
        if (!(*head == line_numbers[i]) && *(++head) != line_numbers[i]) {
            *head = line_numbers[i];
        }
    }
    ++head;
    line_count -= (line_count - (head - line_numbers));

    get_line(fd, line_numbers, line_count);

    free(line_numbers);

    return EXIT_SUCCESS;
}
